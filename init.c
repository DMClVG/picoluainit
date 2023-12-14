#include "boards/pico.h"
#include "hardware/dma.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/sync.h"
#include "pico/assert.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "reent.h"

#include <fcntl.h>
#include <lfs.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern char __fs_end__;
extern char __fs_start__;

#define FS_BASE 0x10080000
#define FS_SIZE (uint32_t) (&__fs_end__ - &__fs_start__)

int
flash_range_read_dma (uint32_t off, void *buf, size_t len)
{
  const void *src = (const void *)(XIP_NOCACHE_NOALLOC_BASE + off);

  const uint dma_channel = 0;
  dma_channel_config dma_cfg = dma_channel_get_default_config (dma_channel);
  channel_config_set_read_increment (&dma_cfg, false);
  channel_config_set_write_increment (&dma_cfg, true);
  channel_config_set_transfer_data_size (&dma_cfg, sizeof (uint8_t));
  dma_channel_configure (dma_channel, &dma_cfg, buf, src, len, true);
  dma_channel_wait_for_finish_blocking (dma_channel);
  return 0;
}

int
read (const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
      void *buffer, lfs_size_t size)
{
  /* flash_range_read_dma (FS_BASE + (block * c->block_size) + off, buffer, */
  /*                       size); */
  memcpy (buffer,
          (const void *)(FS_BASE - XIP_BASE + XIP_NOCACHE_NOALLOC_BASE
                         + block * c->block_size + off),
          size);
  return LFS_ERR_OK;
}

int
prog (const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
      const void *buffer, lfs_size_t size)
{
  int status = save_and_disable_interrupts ();
  flash_range_program ((uint32_t)FS_BASE - XIP_BASE + (block * c->block_size)
                           + off,
                       buffer, size);
  restore_interrupts (status);
  return LFS_ERR_OK;
}

int
erase (const struct lfs_config *c, lfs_block_t block)
{
  int status = save_and_disable_interrupts ();
  flash_range_erase ((uint32_t)FS_BASE - XIP_BASE + (block * c->block_size),
                     c->block_size);
  restore_interrupts (status);
  return LFS_ERR_OK;
}

int
sync (const struct lfs_config *c)
{
  return LFS_ERR_OK;
}

#ifdef LFS_THREADSAFE
#error "THREADS UNSUPPORTED"
#endif

void
fatal_error (const char *error)
{
  while (1)
    {
      printf ("FATAL: %s\n", error);
      sleep_ms (100);
    }
}

/* static int */
/* list_directories (lfs_t *lfs, lfs_dir_t *dir) */
/* { */
/*   static int indent = 0; */
/*   int err = LFS_ERR_OK; */
/*   struct lfs_info info; */
/*   while (1) */
/*     { */
/*       int res = lfs_dir_read (lfs, dir, &info); */
/*       if (res == 0) */
/*         { */
/*           break; */
/*         } */
/*       else if (res < 0) */
/*         { */
/*           err = res; */
/*           goto end; */
/*         } */
/*       printf ("%s\n", info.name); */
/*       if (info.type == LFS_TYPE_DIR) */
/*         { */
/*           indent += 2; */
/*         } */
/*     } */

/* end: */
/*   return err; */
/* } */

static int
lua_panic (lua_State *L)
{
  const char *error = lua_tolstring (L, 1, NULL);
  fatal_error (error);
  return 0;
}

#define LFS_FATAL(code)                                                       \
  {                                                                           \
    int res = code;                                                           \
    if (res < 0)                                                              \
      {                                                                       \
        char msg[128];                                                        \
        snprintf (msg, 128, "%s:%d [LFS] %d", __FILE__, __LINE__, res);       \
        fatal_error (msg);                                                    \
      }                                                                       \
  }

static lfs_t lfs;

int
_write (int file, char *ptr, int len)
{
  return lfs_file_write (&lfs, (lfs_file_t *)file, ptr, len);
}

int
_read (int file, char *ptr, int len)
{
  return lfs_file_read (&lfs, (lfs_file_t *)file, ptr, len);
}

int
_close (int file)
{

  LFS_FATAL (lfs_file_close (&lfs, (lfs_file_t *)file));
  return -1;
}

int
_open (const char *name, int flags, int mode)
{

  lfs_file_t *file = malloc (sizeof (lfs_file_t));
  int rwflags = flags & 0x2;

  int lfsflags = 0;
  if (rwflags == O_RDONLY)
    {
      lfsflags |= LFS_O_RDONLY;
    }
  else if (rwflags == O_WRONLY)
    {
      lfsflags |= LFS_O_WRONLY;
    }
  else if (rwflags == O_RDWR)
    {
      lfsflags |= LFS_O_RDWR;
    }
  if (flags & O_CREAT)
    {
      lfsflags |= LFS_O_CREAT;
    }
  if (flags & O_APPEND)
    {
      lfsflags |= LFS_O_CREAT;
    }
  if (flags & O_EXCL)
    {
      lfsflags |= LFS_O_EXCL;
    }
  if (flags & O_TRUNC)
    {
      lfsflags |= LFS_O_TRUNC;
    }

  if (lfs_file_open (&lfs, file, name, lfsflags) < 0)
    {
      return -1;
    }
  return (int)file;
}

int
_lseek (int file, int off, int whence)
{

  return lfs_file_seek (&lfs, (lfs_file_t *)file, off, whence);
}

int
main ()
{
  stdio_init_all ();
  struct lfs_config config = { .erase = erase,
                               .read = read,
                               .prog = prog,
                               .sync = sync,
                               .read_size = 1,
                               .prog_size = FLASH_PAGE_SIZE,
                               .block_size = FLASH_SECTOR_SIZE,
                               .block_count = FS_SIZE / FLASH_SECTOR_SIZE,
                               .cache_size = FLASH_SECTOR_SIZE / 4,
                               .lookahead_size = 32,
                               .block_cycles = 500 };

  sleep_ms (2000);
  LFS_FATAL (lfs_mount (&lfs, &config));

  // Load init.lua
  lfs_file_t init_file;
  int res = lfs_file_open (&lfs, &init_file, "/init.lua", LFS_O_RDONLY);
  if (res == LFS_ERR_NOENT)
    {
      fatal_error ("Cannot find '/init.lua'. Goodbye.");
    }
  else
    {
      LFS_FATAL (res);
    }

  size_t init_file_size = lfs_file_size (&lfs, &init_file);
  LFS_FATAL (init_file_size);
  char *init = calloc (init_file_size + 1, 1);
  if (!init)
    {
      fatal_error ("");
    }
  LFS_FATAL (lfs_file_read (&lfs, &init_file, init, init_file_size));
  LFS_FATAL (lfs_file_close (&lfs, &init_file));

  lua_State *L = luaL_newstate ();
  lua_atpanic (L, lua_panic);
  luaL_openlibs (L);

  if (luaL_dostring (L, init))
    {
      fatal_error (lua_tostring (L, -1));
    }

  LFS_FATAL (lfs_unmount (&lfs));

  while (1)
    {
      sleep_ms (20);
    }
  return 0;
}
