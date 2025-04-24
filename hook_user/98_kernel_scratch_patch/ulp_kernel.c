
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/uprobes.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/ftrace.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

// 모듈 파라미터
static pid_t target_pid = 0;
module_param(target_pid, int, 0644);
MODULE_PARM_DESC(target_pid, "Target process PID");

static unsigned long target_addr = 0;
module_param(target_addr, ulong, 0644);
MODULE_PARM_DESC(target_addr, "Target function address");

static unsigned long patch_addr = 0;
module_param(patch_addr, ulong, 0644);
MODULE_PARM_DESC(patch_addr, "Patched function address");

// uprobe 구조체
static struct uprobe *uprobe;

// uprobe 핸들러: 함수 진입 시 호출
static int handler_pre(struct uprobe_consumer *self, struct pt_regs *regs)
{
    if (patch_addr) {
        // 패치된 함수로 리다이렉트 (PC 레지스터 수정)
        regs->ip = patch_addr;
        printk(KERN_INFO "Userspace livepatch: Redirecting to patched function at 0x%lx\n", patch_addr);
    } else {
        printk(KERN_INFO "Userspace livepatch: Original function at 0x%lx called in PID %d\n",
               target_addr, task_pid_nr(current));
    }
    return 0;
}

// ioctl 명령
#define ULP_IOC_MAGIC 'u'
#define ULP_IOC_APPLY_PATCH _IOW(ULP_IOC_MAGIC, 1, unsigned long)

static long ulp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (cmd != ULP_IOC_APPLY_PATCH)
        return -EINVAL;

    if (copy_from_user(&patch_addr, (void __user *)arg, sizeof(unsigned long)))
        return -EFAULT;

    printk(KERN_INFO "Userspace livepatch: Patch address set to 0x%lx\n", patch_addr);
    return 0;
}

static const struct file_operations ulp_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = ulp_ioctl,
};

static struct miscdevice ulp_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ulp",
    .fops = &ulp_fops,
};

// 모듈 초기화
static int __init userspace_livepatch_init(void)
{
    struct task_struct *target_task;
    struct pid *pid_struct;
    int ret;

    printk(KERN_INFO "Userspace livepatch: Initializing...\n");

    // PID로 프로세스 찾기
    pid_struct = find_get_pid(target_pid);
    if (!pid_struct) {
        printk(KERN_ERR "Userspace livepatch: Invalid PID %d\n", target_pid);
        return -EINVAL;
    }

    target_task = pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);
    if (!target_task) {
        printk(KERN_ERR "Userspace livepatch: Process with PID %d not found\n", target_pid);
        return -ESRCH;
    }

    // uprobe 구조체 할당
    uprobe = kzalloc(sizeof(struct uprobe), GFP_KERNEL);
    if (!uprobe) {
        printk(KERN_ERR "Userspace livepatch: Failed to allocate uprobe\n");
        return -ENOMEM;
    }

    // uprobe 설정
    uprobe->handler = handler_pre;
    uprobe->offset = target_addr;
    uprobe->inode = target_task->mm->exe_file->f_inode;

    // uprobe 등록
    ret = uprobe_register(uprobe->inode, target_addr, uprobe);
    if (ret) {
        printk(KERN_ERR "Userspace livepatch: Failed to register uprobe: %d\n", ret);
        kfree(uprobe);
        return ret;
    }

    // misc device 등록
    ret = misc_register(&ulp_dev);
    if (ret) {
        printk(KERN_ERR "Userspace livepatch: Failed to register misc device: %d\n", ret);
        uprobe_unregister(uprobe->inode, target_addr, uprobe);
        kfree(uprobe);
        return ret;
    }

    printk(KERN_INFO "Userspace livepatch: Uprobe registered at 0x%lx for PID %d\n",
           target_addr, target_pid);
    return 0;
}

// 모듈 종료
static void __exit userspace_livepatch_exit(void)
{
    if (uprobe) {
        uprobe_unregister(uprobe->inode, target_addr, uprobe);
        kfree(uprobe);
    }
    misc_deregister(&ulp_dev);
    printk(KERN_INFO "Userspace livepatch: Unloaded\n");
}

module_init(userspace_livepatch_init);
module_exit(userspace_livepatch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grok");
MODULE_DESCRIPTION("Userspace livepatch using uprobe and ftrace");
