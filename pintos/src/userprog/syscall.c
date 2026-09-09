#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

/* Guards every access to the file system; see syscall.h. */
struct lock filesys_lock;

/* One entry in a process's open-file table. */
struct fd_entry
  {
    int fd;
    struct file *file;
    struct list_elem elem;
  };

static void syscall_handler (struct intr_frame *);

static void kill_current_process (void) NO_RETURN;
static bool is_mapped_user_addr (const void *uaddr);
static void check_user_ptr (const void *uaddr, size_t size);
static void check_user_string (const char *ustr);
static uint32_t get_syscall_arg (struct intr_frame *f, int idx);

static struct fd_entry *fd_lookup (int fd);

static void sys_halt (void) NO_RETURN;
static void sys_exit (int status) NO_RETURN;
static int sys_exec (const char *cmd_line);
static int sys_wait (tid_t pid);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *file);
static int sys_filesize (int fd);
static int sys_read (int fd, void *buffer, unsigned size);
static int sys_write (int fd, const void *buffer, unsigned size);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);

void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  switch (get_syscall_arg (f, 0))
    {
    case SYS_HALT:
      sys_halt ();

    case SYS_EXIT:
      sys_exit ((int) get_syscall_arg (f, 1));

    case SYS_EXEC:
      f->eax = (uint32_t) sys_exec ((const char *) get_syscall_arg (f, 1));
      break;

    case SYS_WAIT:
      f->eax = (uint32_t) sys_wait ((tid_t) get_syscall_arg (f, 1));
      break;

    case SYS_CREATE:
      f->eax = (uint32_t) sys_create ((const char *) get_syscall_arg (f, 1),
                                       (unsigned) get_syscall_arg (f, 2));
      break;

    case SYS_REMOVE:
      f->eax = (uint32_t) sys_remove ((const char *) get_syscall_arg (f, 1));
      break;

    case SYS_OPEN:
      f->eax = (uint32_t) sys_open ((const char *) get_syscall_arg (f, 1));
      break;

    case SYS_FILESIZE:
      f->eax = (uint32_t) sys_filesize ((int) get_syscall_arg (f, 1));
      break;

    case SYS_READ:
      f->eax = (uint32_t) sys_read ((int) get_syscall_arg (f, 1),
                                     (void *) get_syscall_arg (f, 2),
                                     (unsigned) get_syscall_arg (f, 3));
      break;

    case SYS_WRITE:
      f->eax = (uint32_t) sys_write ((int) get_syscall_arg (f, 1),
                                      (const void *) get_syscall_arg (f, 2),
                                      (unsigned) get_syscall_arg (f, 3));
      break;

    case SYS_SEEK:
      sys_seek ((int) get_syscall_arg (f, 1), (unsigned) get_syscall_arg (f, 2));
      break;

    case SYS_TELL:
      f->eax = (uint32_t) sys_tell ((int) get_syscall_arg (f, 1));
      break;

    case SYS_CLOSE:
      sys_close ((int) get_syscall_arg (f, 1));
      break;

    default:
      kill_current_process ();
    }
}

/* ------------------------------------------------------------ */
/* User memory validation.*/

static void
kill_current_process (void)
{
  thread_current ()->exit_code = -1;
  thread_exit ();
}

static bool
is_mapped_user_addr (const void *uaddr)
{
  return (uaddr != NULL && is_user_vaddr (uaddr)
          && pagedir_get_page (thread_current ()->pagedir, uaddr) != NULL);
}

/* Verifies that the SIZE bytes starting at UADDR are entirely
   mapped user memory, killing the current process if not. SIZE
   must be at least 1. */
static void
check_user_ptr (const void *uaddr, size_t size)
{
  const uint8_t *start = uaddr;
  const uint8_t *end = start + (size - 1);
  const uint8_t *page;

  if (end < start)
    kill_current_process ();  /* SIZE is absurd enough to wrap the address space. */

  for (page = pg_round_down (start); page <= end; page += PGSIZE)
    if (!is_mapped_user_addr (page))
      kill_current_process ();
}

/* Verifies that USTR is a mapped, NUL-terminated user string,
   killing the current process if it runs off into invalid memory
   before finding the terminator. */
static void
check_user_string (const char *ustr)
{
  const char *p = ustr;

  for (;;)
    {
      check_user_ptr (p, 1);
      if (*p == '\0')
        break;
      p++;
    }
}

/* Fetches the IDXth 32-bit word above the system call number on
   the interrupt frame's user stack (IDX 0 is the call number
   itself, IDX 1 the first argument, and so on), validating it
   first. */
static uint32_t
get_syscall_arg (struct intr_frame *f, int idx)
{
  uint32_t *addr = (uint32_t *) f->esp + idx;
  check_user_ptr (addr, sizeof *addr);
  return *addr;
}

/* ------------------------------------------------------------ */
/* Process control. */

static void
sys_halt (void)
{
  shutdown_power_off ();
}

static void
sys_exit (int status)
{
  thread_current ()->exit_code = status;
  thread_exit ();
}

static int
sys_exec (const char *cmd_line)
{
  tid_t tid;

  check_user_string (cmd_line);

  tid = process_execute (cmd_line);
  return tid == TID_ERROR ? -1 : tid;
}

static int
sys_wait (tid_t pid)
{
  return process_wait (pid);
}

/* ------------------------------------------------------------ */
/* File descriptor table. */

static struct fd_entry *
fd_lookup (int fd)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds); e = list_next (e))
    {
      struct fd_entry *fe = list_entry (e, struct fd_entry, elem);
      if (fe->fd == fd)
        return fe;
    }
  return NULL;
}

void
syscall_close_all_fds (void)
{
  struct thread *cur = thread_current ();

  while (!list_empty (&cur->fds))
    {
      struct fd_entry *fe = list_entry (list_pop_front (&cur->fds),
                                         struct fd_entry, elem);
      lock_acquire (&filesys_lock);
      file_close (fe->file);
      lock_release (&filesys_lock);
      free (fe);
    }
}

/* ------------------------------------------------------------ */
/* File system calls. */

static bool
sys_create (const char *file, unsigned initial_size)
{
  bool ok;

  check_user_string (file);

  lock_acquire (&filesys_lock);
  ok = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return ok;
}

static bool
sys_remove (const char *file)
{
  bool ok;

  check_user_string (file);

  lock_acquire (&filesys_lock);
  ok = filesys_remove (file);
  lock_release (&filesys_lock);
  return ok;
}

static int
sys_open (const char *file)
{
  struct file *f;
  struct fd_entry *fe;
  struct thread *cur;
  int fd;

  check_user_string (file);

  lock_acquire (&filesys_lock);
  f = filesys_open (file);
  lock_release (&filesys_lock);
  if (f == NULL)
    return -1;

  fe = malloc (sizeof *fe);
  if (fe == NULL)
    {
      lock_acquire (&filesys_lock);
      file_close (f);
      lock_release (&filesys_lock);
      return -1;
    }

  cur = thread_current ();
  fd = cur->next_fd++;
  fe->fd = fd;
  fe->file = f;
  list_push_back (&cur->fds, &fe->elem);
  return fd;
}

static int
sys_filesize (int fd)
{
  struct fd_entry *fe = fd_lookup (fd);
  int size;

  if (fe == NULL)
    return -1;

  lock_acquire (&filesys_lock);
  size = file_length (fe->file);
  lock_release (&filesys_lock);
  return size;
}

static int
sys_read (int fd, void *buffer, unsigned size)
{
  struct fd_entry *fe;
  int bytes;

  check_user_ptr (buffer, size == 0 ? 1 : size);

  if (fd == STDIN_FILENO)
    {
      uint8_t *buf = buffer;
      unsigned i;
      for (i = 0; i < size; i++)
        buf[i] = input_getc ();
      return size;
    }

  if (fd == STDOUT_FILENO)
    return -1;

  fe = fd_lookup (fd);
  if (fe == NULL)
    return -1;

  lock_acquire (&filesys_lock);
  bytes = file_read (fe->file, buffer, size);
  lock_release (&filesys_lock);
  return bytes;
}

static int
sys_write (int fd, const void *buffer, unsigned size)
{
  struct fd_entry *fe;
  int bytes;

  check_user_ptr (buffer, size == 0 ? 1 : size);

  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      return size;
    }

  if (fd == STDIN_FILENO)
    return -1;

  fe = fd_lookup (fd);
  if (fe == NULL)
    return -1;

  lock_acquire (&filesys_lock);
  bytes = file_write (fe->file, buffer, size);
  lock_release (&filesys_lock);
  return bytes;
}

static void
sys_seek (int fd, unsigned position)
{
  struct fd_entry *fe = fd_lookup (fd);

  if (fe == NULL)
    return;

  lock_acquire (&filesys_lock);
  file_seek (fe->file, position);
  lock_release (&filesys_lock);
}

static unsigned
sys_tell (int fd)
{
  struct fd_entry *fe = fd_lookup (fd);
  unsigned pos;

  if (fe == NULL)
    return 0;

  lock_acquire (&filesys_lock);
  pos = file_tell (fe->file);
  lock_release (&filesys_lock);
  return pos;
}

static void
sys_close (int fd)
{
  struct fd_entry *fe = fd_lookup (fd);

  if (fe == NULL)
    return;

  list_remove (&fe->elem);
  lock_acquire (&filesys_lock);
  file_close (fe->file);
  lock_release (&filesys_lock);
  free (fe);
}
