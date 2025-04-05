// Check compressed file with the below command.
// python3 -c "import zlib; print(zlib.decompress(open('hi.raw.zlib', 'rb').read()).decode('utf-8'))"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/zlib.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#define PROC_NAME           "compress_single"
#define BUFFER_SIZE         4096
#define MAX_PATH_LEN        256
#define RAW_ZLIB_SUFFIX     ".raw.zlib"

static struct proc_dir_entry *proc_entry;
static char target_path[MAX_PATH_LEN];
static char compressed_path[MAX_PATH_LEN + sizeof(RAW_ZLIB_SUFFIX)];

struct compress_context {
    unsigned char *input_buf;
    unsigned char *output_buf;
    z_stream stream;
    struct file *filp;
};

static void free_context(struct compress_context *ctx)
{
    if (ctx->filp)
        filp_close(ctx->filp, NULL);
    kfree(ctx->input_buf);
    kfree(ctx->output_buf);
    kfree(ctx->stream.workspace);
}

static int init_context(struct compress_context *ctx)
{
    ctx->input_buf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->input_buf)
        return -ENOMEM;

    ctx->output_buf = kmalloc(BUFFER_SIZE * 2, GFP_KERNEL);
    if (!ctx->output_buf)
        return -ENOMEM;

    ctx->stream.workspace = kmalloc(zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL), GFP_KERNEL);
    if (!ctx->stream.workspace)
        return -ENOMEM;

    ctx->filp = NULL;
    return 0;
}

static int compress_data(struct compress_context *ctx, ssize_t bytes_read)
{
    int ret;

    ret = zlib_deflateInit(&ctx->stream, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        pr_err("zlib_deflateInit failed: %d\n", ret);
        return ret;
    }

    ctx->stream.next_in = ctx->input_buf;
    ctx->stream.avail_in = bytes_read;
    ctx->stream.next_out = ctx->output_buf;
    ctx->stream.avail_out = BUFFER_SIZE * 2;

    ret = zlib_deflate(&ctx->stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        pr_err("zlib_deflate failed: %d\n", ret);
        zlib_deflateEnd(&ctx->stream);
        return ret;
    }

    pr_info("Original size: %ld, Compressed size: %lu\n", bytes_read, ctx->stream.total_out);
    zlib_deflateEnd(&ctx->stream);
    return 0;
}

static ssize_t compress_file(void)
{
    struct compress_context ctx = {0};
    ssize_t bytes_read;
    loff_t pos = 0;
    int ret;
    size_t suffix_len = strlen(RAW_ZLIB_SUFFIX);

    // Construct compressed file path using strscpy
    ret = strscpy(compressed_path, target_path, MAX_PATH_LEN);
    if (ret < 0) {
        pr_err("Path too long: %s\n", target_path);
        return -ENAMETOOLONG;
    }

    // Check if there's enough space for the suffix
    if (ret + suffix_len >= sizeof(compressed_path)) {
        pr_err("Path with suffix too long: %s\n", target_path);
        return -ENAMETOOLONG;
    }

    strscpy(compressed_path + ret, RAW_ZLIB_SUFFIX, sizeof(compressed_path) - ret);

    // Initialize context
    ret = init_context(&ctx);
    if (ret)
        goto cleanup;

    // Open file for reading
    ctx.filp = filp_open(target_path, O_RDONLY, 0);
    if (IS_ERR(ctx.filp)) {
        pr_err("Failed to open file: %s\n", target_path);
        ret = PTR_ERR(ctx.filp);
        ctx.filp = NULL;
        goto cleanup;
    }

    // Read file
    bytes_read = kernel_read(ctx.filp, ctx.input_buf, BUFFER_SIZE, &pos);
    if (bytes_read < 0) {
        pr_err("Failed to read file: %ld\n", bytes_read);
        ret = bytes_read;
        goto cleanup;
    }

    // Close input file
    filp_close(ctx.filp, NULL);
    ctx.filp = NULL;

    // Compress data
    ret = compress_data(&ctx, bytes_read);
    if (ret)
        goto cleanup;

    // Open file for writing with new name
    ctx.filp = filp_open(compressed_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(ctx.filp)) {
        pr_err("Failed to open file for writing: %s\n", compressed_path);
        ret = PTR_ERR(ctx.filp);
        ctx.filp = NULL;
        goto cleanup;
    }

    // Write compressed data
    pos = 0;
    ret = kernel_write(ctx.filp, ctx.output_buf, ctx.stream.total_out, &pos);
    if (ret < 0) {
        pr_err("Failed to write compressed data: %d\n", ret);
    } else {
        pr_info("Successfully wrote %d bytes to %s\n", ret, compressed_path);
    }

cleanup:
    free_context(&ctx);
    return ret < 0 ? ret : bytes_read;
}

static ssize_t proc_write(struct file *file, const char __user *buffer,
                          size_t count, loff_t *pos)
{
    if (count > MAX_PATH_LEN - 1)
        count = MAX_PATH_LEN - 1;

    if (copy_from_user(target_path, buffer, count)) {
        pr_err("Failed to copy file path from user\n");
        return -EFAULT;
    }

    target_path[count] = '\0';
    if (target_path[count - 1] == '\n')
        target_path[count - 1] = '\0';

    pr_info("Received file path: %s\n", target_path);
    return compress_file() < 0 ? -EIO : count;
}

static const struct proc_ops proc_fops = {
    .proc_write = proc_write,
};

static int __init compress_init(void)
{
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("Failed to create proc entry\n");
        return -ENOMEM;
    }

    pr_info("Compression module loaded\n");
    return 0;
}

static void __exit compress_exit(void)
{
    proc_remove(proc_entry);
    pr_info("Compression module unloaded\n");
}

module_init(compress_init);
module_exit(compress_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Single file compression module with .raw.zlib output");
