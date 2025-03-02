/***************************************************************************
 * Copyright (C) 2016
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "iosuhax.h"
#include "os_functions.h"
#include <malloc.h>
#include <string.h>

#define IOSUHAX_MAGIC_WORD      0x4E696365

#define IOCTL_MEM_WRITE         0x00
#define IOCTL_MEM_READ          0x01
#define IOCTL_SVC               0x02
#define IOCTL_MEMCPY            0x04
#define IOCTL_REPEATED_WRITE    0x05
#define IOCTL_KERN_READ32       0x06
#define IOCTL_KERN_WRITE32      0x07
#define IOCTL_READ_OTP          0x08

#define IOCTL_FSA_OPEN          0x40
#define IOCTL_FSA_CLOSE         0x41
#define IOCTL_FSA_MOUNT         0x42
#define IOCTL_FSA_UNMOUNT       0x43
#define IOCTL_FSA_GETDEVICEINFO 0x44
#define IOCTL_FSA_OPENDIR       0x45
#define IOCTL_FSA_READDIR       0x46
#define IOCTL_FSA_CLOSEDIR      0x47
#define IOCTL_FSA_MAKEDIR       0x48
#define IOCTL_FSA_OPENFILE      0x49
#define IOCTL_FSA_READFILE      0x4A
#define IOCTL_FSA_WRITEFILE     0x4B
#define IOCTL_FSA_STATFILE      0x4C
#define IOCTL_FSA_CLOSEFILE     0x4D
#define IOCTL_FSA_SETFILEPOS    0x4E
#define IOCTL_FSA_GETSTAT       0x4F
#define IOCTL_FSA_REMOVE        0x50
#define IOCTL_FSA_REWINDDIR     0x51
#define IOCTL_FSA_CHDIR         0x52
#define IOCTL_FSA_RENAME        0x53
#define IOCTL_FSA_RAW_OPEN      0x54
#define IOCTL_FSA_RAW_READ      0x55
#define IOCTL_FSA_RAW_WRITE     0x56
#define IOCTL_FSA_RAW_CLOSE     0x57
#define IOCTL_FSA_CHANGEMODE    0x58
#define IOCTL_FSA_FLUSHVOLUME   0x59
#define IOCTL_CHECK_IF_IOSUHAX  0x5B

static int iosuhaxHandle = -1;

#define ALIGN(align)      __attribute__((aligned(align)))
#define ALIGN_0x20        ALIGN(0x20)
#define ROUNDUP(x, align) (((x) + ((align) -1)) & ~((align) -1))

int IOSUHAX_Open(const char *dev) {
    if (iosuhaxHandle >= 0)
        return iosuhaxHandle;

    iosuhaxHandle = IOS_Open((char *) (dev ? dev : "/dev/iosuhax"), 0);
    if (iosuhaxHandle >= 0 && dev) //make sure device is actually iosuhax
    {
        ALIGN_0x20 int res[0x20 >> 2];
        *res = 0;

        IOS_Ioctl(iosuhaxHandle, IOCTL_CHECK_IF_IOSUHAX, (void *) 0, 0, res, 4);
        if (*res != IOSUHAX_MAGIC_WORD) {
            IOS_Close(iosuhaxHandle);
            iosuhaxHandle = -1;
        }
    }

    return iosuhaxHandle;
}

int IOSUHAX_Close(void) {
    if (iosuhaxHandle < 0)
        return 0;

    int res       = IOS_Close(iosuhaxHandle);
    iosuhaxHandle = -1;
    return res;
}

int IOSUHAX_memwrite(uint32_t address, const uint8_t *buffer, uint32_t size) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, ROUNDUP(size + 4, 0x20));
    if (!io_buf)
        return -2;

    io_buf[0] = address;
    memcpy(io_buf + 1, buffer, size);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_MEM_WRITE, io_buf, size + 4, 0, 0);

    free(io_buf);
    return res;
}

int IOSUHAX_ODM_GetDiscKey(uint8_t *discKey) {
    int res = -1;
    if (discKey == NULL) {
        return -2;
    }
    int odm_handle = IOS_Open("/dev/odm", 1);
    res            = odm_handle;
    if (odm_handle >= 0) {
        uint32_t io_buffer[0x20 / 4];
        // disc encryption key, only works with patched IOSU
        io_buffer[0] = 3;
        res          = IOS_Ioctl(odm_handle, 0x06, io_buffer, 0x14, io_buffer, 0x20);
        if (res == 0) {
            memcpy(discKey, io_buffer, 16);
        }
        IOS_Close(odm_handle);
    }
    return res;
}

int IOSUHAX_memread(uint32_t address, uint8_t *out_buffer, uint32_t size) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    ALIGN_0x20 int io_buf[0x20 >> 2];
    io_buf[0] = address;

    void *tmp_buf = NULL;

    if (((uintptr_t) out_buffer & 0x1F) || (size & 0x1F)) {
        tmp_buf = (uint32_t *) memalign(0x20, ROUNDUP(size, 0x20));
        if (!tmp_buf)
            return -2;
    }

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_MEM_READ, io_buf, sizeof(address), tmp_buf ? tmp_buf : out_buffer, size);

    if (res >= 0 && tmp_buf)
        memcpy(out_buffer, tmp_buf, size);

    free(tmp_buf);
    return res;
}

int IOSUHAX_memcpy(uint32_t dst, uint32_t src, uint32_t size) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    ALIGN_0x20 uint32_t io_buf[0x20 >> 2];
    io_buf[0] = dst;
    io_buf[1] = src;
    io_buf[2] = size;

    return IOS_Ioctl(iosuhaxHandle, IOCTL_MEMCPY, io_buf, 3 * sizeof(uint32_t), 0, 0);
}

int IOSUHAX_kern_write32(uint32_t address, uint32_t value) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    ALIGN_0x20 uint32_t io_buf[0x20 >> 2];
    io_buf[0] = address;
    io_buf[1] = value;

    return IOS_Ioctl(iosuhaxHandle, IOCTL_KERN_WRITE32, io_buf, 2 * sizeof(uint32_t), 0, 0);
}

int IOSUHAX_read_otp(uint8_t *out_buffer, uint32_t size) {
    if (iosuhaxHandle < 0) {
        return iosuhaxHandle;
    }

    ALIGN_0x20 uint32_t io_buf[0x400 >> 2];

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_READ_OTP, 0, 0, io_buf, 0x400);

    if (res >= 0) {
        memcpy(out_buffer, io_buf, size > 0x400 ? 0x400 : size);
    }

    return res;
}

extern int bspRead(const char *, uint32_t, const char *, uint32_t, uint16_t *);

int IOSUHAX_read_seeprom(uint8_t *out_buffer, uint32_t offset, uint32_t size) {
    if (out_buffer == NULL || offset > 0x200 || offset & 0x01) {
        return -1;
    }

    uint32_t sizeInShorts   = size >> 1;
    uint32_t offsetInShorts = offset >> 1;
    int32_t maxReadCount    = 0x100 - offsetInShorts;

    if (maxReadCount <= 0) {
        return 0;
    }

    uint32_t count = sizeInShorts > maxReadCount ? maxReadCount : sizeInShorts;
    uint16_t *ptr  = (uint16_t *) out_buffer;

    int res = 0;

    for (int i = 0; i < count; i++) {
        if (bspRead("EE", offsetInShorts + i, "access", 2, ptr) != 0) {
            return -2;
        }
        res += 2;
        ptr++;
    }

    return res;
}

int IOSUHAX_kern_read32(uint32_t address, uint32_t *out_buffer, uint32_t count) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    ALIGN_0x20 uint32_t io_buf[0x20 >> 2];
    io_buf[0] = address;

    void *tmp_buf = NULL;

    if (((uintptr_t) out_buffer & 0x1F) || ((count * 4) & 0x1F)) {
        tmp_buf = (uint32_t *) memalign(0x20, ROUNDUP((count * 4), 0x20));
        if (!tmp_buf)
            return -2;
    }

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_KERN_READ32, io_buf, sizeof(address), tmp_buf ? tmp_buf : out_buffer, count * 4);

    if (res >= 0 && tmp_buf)
        memcpy(out_buffer, tmp_buf, count * 4);

    free(tmp_buf);
    return res;
}

int IOSUHAX_SVC(uint32_t svc_id, uint32_t *args, uint32_t arg_cnt) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    ALIGN_0x20 uint32_t arguments[0x40 >> 2];
    arguments[0] = svc_id;

    if (args && arg_cnt) {
        if (arg_cnt > 8)
            arg_cnt = 8;

        memcpy(arguments + 1, args, arg_cnt * 4);
    }

    ALIGN_0x20 int result[0x20 >> 2];
    int ret = IOS_Ioctl(iosuhaxHandle, IOCTL_SVC, arguments, (1 + arg_cnt) * 4, result, 4);
    if (ret < 0)
        return ret;

    return *result;
}

int IOSUHAX_FSA_Open(void) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    ALIGN_0x20 int io_buf[0x20 >> 2];

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_OPEN, 0, 0, io_buf, sizeof(int));
    if (res < 0)
        return res;

    return io_buf[0];
}

int IOSUHAX_FSA_Close(int fsaFd) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    ALIGN_0x20 int io_buf[0x20 >> 2];
    io_buf[0] = fsaFd;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_CLOSE, io_buf, sizeof(fsaFd), io_buf, sizeof(fsaFd));
    if (res < 0)
        return res;

    return io_buf[0];
}

int IOSUHAX_FSA_Mount(int fsaFd, const char *device_path, const char *volume_path, uint32_t flags, const char *arg_string, int arg_string_len) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 6;

    int io_buf_size = (sizeof(uint32_t) * input_cnt) + strlen(device_path) + strlen(volume_path) + arg_string_len + 3;

    ALIGN_0x20 int io_buf[ROUNDUP(io_buf_size, 0x20) >> 2];
    memset(io_buf, 0, io_buf_size);

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    io_buf[2] = io_buf[1] + strlen(device_path) + 1;
    io_buf[3] = flags;
    io_buf[4] = arg_string_len ? (io_buf[2] + strlen(volume_path) + 1) : 0;
    io_buf[5] = arg_string_len;

    strcpy(((char *) io_buf) + io_buf[1], device_path);
    strcpy(((char *) io_buf) + io_buf[2], volume_path);

    if (arg_string_len)
        memcpy(((char *) io_buf) + io_buf[4], arg_string, arg_string_len);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_MOUNT, io_buf, io_buf_size, io_buf, 4);
    if (res < 0)
        return res;

    return io_buf[0];
}

int IOSUHAX_FSA_Unmount(int fsaFd, const char *path, uint32_t flags) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 3;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + 1;

    ALIGN_0x20 int io_buf[ROUNDUP(io_buf_size, 0x20) >> 2];

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    io_buf[2] = flags;
    strcpy(((char *) io_buf) + io_buf[1], path);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_UNMOUNT, io_buf, io_buf_size, io_buf, 4);
    if (res < 0)
        return res;

    return io_buf[0];
}

int IOSUHAX_FSA_FlushVolume(int fsaFd, const char *volume_path) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(volume_path) + 1;

    ALIGN_0x20 int io_buf[ROUNDUP(io_buf_size, 0x20) >> 2];

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    strcpy(((char *) io_buf) + io_buf[1], volume_path);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_FLUSHVOLUME, io_buf, io_buf_size, io_buf, 4);
    if (res < 0)
        return res;

    return io_buf[0];
}

int IOSUHAX_FSA_GetDeviceInfo(int fsaFd, const char *device_path, int type, uint32_t *out_data) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 3;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(device_path) + 1;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    io_buf[2] = type;
    strcpy(((char *) io_buf) + io_buf[1], device_path);

    uint32_t out_buf[1 + 0x64 / 4];

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_GETDEVICEINFO, io_buf, io_buf_size, out_buf, sizeof(out_buf));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    memcpy(out_data, out_buf + 1, 0x64);
    free(io_buf);
    return out_buf[0];
}

int IOSUHAX_FSA_MakeDir(int fsaFd, const char *path, uint32_t flags) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 3;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + 1;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    io_buf[2] = flags;
    strcpy(((char *) io_buf) + io_buf[1], path);

    int result;
    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_MAKEDIR, io_buf, io_buf_size, &result, sizeof(result));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    free(io_buf);
    return result;
}

int IOSUHAX_FSA_OpenDir(int fsaFd, const char *path, int *outHandle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + 1;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    strcpy(((char *) io_buf) + io_buf[1], path);

    int result_vec[2];

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_OPENDIR, io_buf, io_buf_size, result_vec, sizeof(result_vec));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    *outHandle = result_vec[1];
    free(io_buf);
    return result_vec[0];
}

int IOSUHAX_FSA_ReadDir(int fsaFd, int handle, FSDirectoryEntry *out_data) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = handle;

    int result_vec_size = 4 + sizeof(FSDirectoryEntry);
    uint8_t *result_vec = (uint8_t *) memalign(0x20, result_vec_size);
    if (!result_vec) {
        free(io_buf);
        return -2;
    }

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_READDIR, io_buf, io_buf_size, result_vec, result_vec_size);
    if (res < 0) {
        free(result_vec);
        free(io_buf);
        return res;
    }

    int result = *(int *) result_vec;
    memcpy(out_data, result_vec + 4, sizeof(FSDirectoryEntry));

    // Force FS_STAT_FILE when a size is set.
    if ((out_data->info.flags & FS_STAT_DIRECTORY) != FS_STAT_DIRECTORY && out_data->info.size > 0) {
        out_data->info.flags |= FS_STAT_FILE;
    }

    free(io_buf);
    free(result_vec);
    return result;
}

int IOSUHAX_FSA_RewindDir(int fsaFd, int dirHandle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = dirHandle;

    int result;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_REWINDDIR, io_buf, io_buf_size, &result, sizeof(result));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    free(io_buf);
    return result;
}

int IOSUHAX_FSA_CloseDir(int fsaFd, int handle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = handle;

    int result;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_CLOSEDIR, io_buf, io_buf_size, &result, sizeof(result));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    free(io_buf);
    return result;
}

int IOSUHAX_FSA_ChangeDir(int fsaFd, const char *path) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + 1;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    strcpy(((char *) io_buf) + io_buf[1], path);

    int result;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_CHDIR, io_buf, io_buf_size, &result, sizeof(result));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    free(io_buf);
    return result;
}

int IOSUHAX_FSA_OpenFile(int fsaFd, const char *path, const char *mode, int *outHandle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 3;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + strlen(mode) + 2;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    io_buf[2] = io_buf[1] + strlen(path) + 1;
    strcpy(((char *) io_buf) + io_buf[1], path);
    strcpy(((char *) io_buf) + io_buf[2], mode);

    int result_vec[2];

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_OPENFILE, io_buf, io_buf_size, result_vec, sizeof(result_vec));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    *outHandle = result_vec[1];
    free(io_buf);
    return result_vec[0];
}

int IOSUHAX_FSA_ReadFile(int fsaFd, void *data, uint32_t size, uint32_t cnt, int fileHandle, uint32_t flags) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 5;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = size;
    io_buf[2] = cnt;
    io_buf[3] = fileHandle;
    io_buf[4] = flags;

    int out_buf_size = ((size * cnt + 0x40) + 0x3F) & ~0x3F;

    uint32_t *out_buffer = (uint32_t *) memalign(0x40, out_buf_size);
    if (!out_buffer) {
        free(io_buf);
        return -2;
    }

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_READFILE, io_buf, io_buf_size, out_buffer, out_buf_size);
    if (res < 0) {
        free(out_buffer);
        free(io_buf);
        return res;
    }

    //! data is put to offset 0x40 to align the buffer output
    memcpy(data, ((uint8_t *) out_buffer) + 0x40, size * cnt);

    int result = out_buffer[0];

    free(out_buffer);
    free(io_buf);
    return result;
}

int IOSUHAX_FSA_WriteFile(int fsaFd, const void *data, uint32_t size, uint32_t cnt, int fileHandle, uint32_t flags) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 5;

    int io_buf_size = ((sizeof(uint32_t) * input_cnt + size * cnt + 0x40) + 0x3F) & ~0x3F;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = size;
    io_buf[2] = cnt;
    io_buf[3] = fileHandle;
    io_buf[4] = flags;

    //! data is put to offset 0x40 to align the buffer input
    memcpy(((uint8_t *) io_buf) + 0x40, data, size * cnt);

    int result;
    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_WRITEFILE, io_buf, io_buf_size, &result, sizeof(result));
    if (res < 0) {
        free(io_buf);
        return res;
    }
    free(io_buf);
    return result;
}

int IOSUHAX_FSA_StatFile(int fsaFd, int fileHandle, FSStat *out_data) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = fileHandle;

    int out_buf_size     = 4 + sizeof(FSStat);
    uint32_t *out_buffer = (uint32_t *) memalign(0x20, out_buf_size);
    if (!out_buffer) {
        free(io_buf);
        return -2;
    }

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_STATFILE, io_buf, io_buf_size, out_buffer, out_buf_size);
    if (res < 0) {
        free(io_buf);
        free(out_buffer);
        return res;
    }

    int result = out_buffer[0];
    memcpy(out_data, out_buffer + 1, sizeof(FSStat));

    // Force FS_STAT_FILE when a size is set.
    if ((out_data->flags & FS_STAT_DIRECTORY) != FS_STAT_DIRECTORY && out_data->size > 0) {
        out_data->flags |= FS_STAT_FILE;
    }

    free(io_buf);
    free(out_buffer);
    return result;
}

int IOSUHAX_FSA_CloseFile(int fsaFd, int fileHandle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = fileHandle;

    int result;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_CLOSEFILE, io_buf, io_buf_size, &result, sizeof(result));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    free(io_buf);
    return result;
}

int IOSUHAX_FSA_SetFilePos(int fsaFd, int fileHandle, uint32_t position) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 3;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = fileHandle;
    io_buf[2] = position;

    int result;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_SETFILEPOS, io_buf, io_buf_size, &result, sizeof(result));
    if (res < 0) {
        free(io_buf);
        return res;
    }

    free(io_buf);
    return result;
}

int IOSUHAX_FSA_GetStat(int fsaFd, const char *path, FSStat *out_data) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + 1;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    strcpy(((char *) io_buf) + io_buf[1], path);

    int out_buf_size     = 4 + sizeof(FSStat);
    uint32_t *out_buffer = (uint32_t *) memalign(0x20, out_buf_size);
    if (!out_buffer) {
        free(io_buf);
        return -2;
    }

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_GETSTAT, io_buf, io_buf_size, out_buffer, out_buf_size);
    if (res < 0) {
        free(io_buf);
        free(out_buffer);
        return res;
    }

    int result = out_buffer[0];
    memcpy(out_data, out_buffer + 1, sizeof(FSStat));

    // Force FS_STAT_FILE when a size is set.
    if ((out_data->flags & FS_STAT_DIRECTORY) != FS_STAT_DIRECTORY && out_data->size > 0) {
        out_data->flags |= FS_STAT_FILE;
    }

    free(io_buf);
    free(out_buffer);
    return result;
}

int IOSUHAX_FSA_Remove(int fsaFd, const char *path) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + 1;

    uint32_t *io_buf = (uint32_t *) memalign(0x20, ROUNDUP(io_buf_size, 0x20));
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    strcpy(((char *) io_buf) + io_buf[1], path);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_REMOVE, io_buf, io_buf_size, io_buf, 4);
    if (res >= 0)
        res = io_buf[0];

    free(io_buf);
    return res;
}

int IOSUHAX_FSA_ChangeMode(int fsaFd, const char *path, int mode) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 3;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(path) + 1;

    ALIGN_0x20 uint32_t io_buf[ROUNDUP(io_buf_size, 0x20) >> 2];

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    io_buf[2] = mode;
    strcpy(((char *) io_buf) + io_buf[1], path);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_CHANGEMODE, io_buf, io_buf_size, io_buf, 4);
    if (res < 0)
        return res;

    return io_buf[0];
}

int IOSUHAX_FSA_RawOpen(int fsaFd, const char *device_path, int *outHandle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt + strlen(device_path) + 1;

    ALIGN_0x20 uint32_t io_buf[ROUNDUP(io_buf_size, 0x20) >> 2];

    io_buf[0] = fsaFd;
    io_buf[1] = sizeof(uint32_t) * input_cnt;
    strcpy(((char *) io_buf) + io_buf[1], device_path);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_RAW_OPEN, io_buf, io_buf_size, io_buf, 2 * sizeof(int));
    if (res < 0)
        return res;

    if (outHandle)
        *outHandle = io_buf[1];

    return io_buf[0];
}

int IOSUHAX_FSA_RawRead(int fsaFd, void *data, uint32_t block_size, uint32_t block_cnt, uint64_t sector_offset, int device_handle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 6;

    int io_buf_size  = 0x40 + block_size * block_cnt;
    uint32_t *io_buf = (uint32_t *) memalign(0x40, ROUNDUP(io_buf_size, 0x40));

    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = block_size;
    io_buf[2] = block_cnt;
    io_buf[3] = (sector_offset >> 32) & 0xFFFFFFFF;
    io_buf[4] = sector_offset & 0xFFFFFFFF;
    io_buf[5] = device_handle;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_RAW_READ, io_buf, sizeof(uint32_t) * input_cnt, io_buf, io_buf_size);
    if (res >= 0) {
        //! data is put to offset 0x40 to align the buffer output
        memcpy(data, ((uint8_t *) io_buf) + 0x40, block_size * block_cnt);

        res = io_buf[0];
    }

    free(io_buf);
    return res;
}

int IOSUHAX_FSA_RawWrite(int fsaFd, const void *data, uint32_t block_size, uint32_t block_cnt, uint64_t sector_offset, int device_handle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    int io_buf_size = ROUNDUP(0x40 + block_size * block_cnt, 0x40);

    uint32_t *io_buf = (uint32_t *) memalign(0x40, io_buf_size);
    if (!io_buf)
        return -2;

    io_buf[0] = fsaFd;
    io_buf[1] = block_size;
    io_buf[2] = block_cnt;
    io_buf[3] = (sector_offset >> 32) & 0xFFFFFFFF;
    io_buf[4] = sector_offset & 0xFFFFFFFF;
    io_buf[5] = device_handle;

    //! data is put to offset 0x40 to align the buffer input
    memcpy(((uint8_t *) io_buf) + 0x40, data, block_size * block_cnt);

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_RAW_WRITE, io_buf, io_buf_size, io_buf, 4);
    if (res >= 0)
        res = io_buf[0];

    free(io_buf);
    return res;
}


int IOSUHAX_FSA_RawClose(int fsaFd, int device_handle) {
    if (iosuhaxHandle < 0)
        return iosuhaxHandle;

    const int input_cnt = 2;

    int io_buf_size = sizeof(uint32_t) * input_cnt;

    ALIGN_0x20 uint32_t io_buf[ROUNDUP(io_buf_size, 0x20) >> 2];

    io_buf[0] = fsaFd;
    io_buf[1] = device_handle;

    int res = IOS_Ioctl(iosuhaxHandle, IOCTL_FSA_RAW_CLOSE, io_buf, io_buf_size, io_buf, 4);
    if (res < 0)
        return res;

    return io_buf[0];
}
