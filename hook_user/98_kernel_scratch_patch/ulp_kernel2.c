#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/uprobes.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/ftrace.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

// 모듈 파라미터
static pid_t target_pid = 0;
module_param(target_pid, int, 0644);
MODULE_PARM_DESC(target_pid, "Target process PID");

static unsigned long target_addr = 0;
module_param(target_addr, ulong, 0644);
MODULE_PARM_DESC(target_addr, "Target function address");

static unsigned long patch_addr = 0;
module_param(patch_addr, ulong, 0644);
MODULE_PARM_DESC(patch_addr, "Patch function address");

// uprobe 구조체
static struct uprobe *uprobe;

// uprobe 핸들러: 함수 진입 시 호출
static int handler_pre(struct uprobe_consumer *self, struct pt_regs *regs)
{
    // 패치된 함수로 리다이렉트 (레지스터 조작)
    if (patch_addr) {
        regs->ip = patch_addr; // RIP를 패치된 함수 주소로 변경
        printk(KERN_INFO "ULP: Redirecting to patched function at 0x%lx\n", patch_addr);
    } else {
        printk(KERN_INFO "ULP: Hooked function at 0x%lx in PID %d\n",
               target_addr, task_pid_nr(current));
    }
    return 0;
}

// ftrace 이벤트 설정 (디버깅용)
static void notrace ulp_ftrace_callback(unsigned long ip, unsigned long parent_ip,
                                       struct ftrace_ops *ops, struct pt_regs *regs)
{
    printk(KERN_INFO "ULP: ftrace event at 0x%lx (parent: 0x%lx)\n", ip, parent_ip);
}

static struct ftrace_ops ulp_ftrace_ops = {
    .func = ulp_ftrace_callback,
    .flags = FTRACE_OPS_FL_SAVE_REGS,
};

// misc device for userspace control
static long ulp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case 0x1001: // Apply patch
        patch_addr = arg;
        printk(KERN_INFO "ULP: Patch address set to 0x%lx\n", patch_addr);
        break;
    case 0x1002: // Remove patch
        patch_addr = 0;
        printk(KERN_INFO "ULP: Patch removed\n");
        break;
    default:
        return -EINVAL;
    }
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
static int __init ulp_init(void)
{
    struct task_struct *target_task;
    struct pid *pid_struct;
    int ret;

    printk(KERN_INFO "ULP: Initializing...\n");

    // PID로 프로세스 찾기
    pid_struct = find_get_pid(target_pid);
    if (!pid_struct) {
        printk(KERN_ERR "ULP: Invalid PID %d\n", target_pid);
        return -EINVAL;
    }

    target_task = pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);
    if (!target_task) {
        printk(KERN_ERR "ULP: Process with PID %d not found\n", target_pid);
        return -ESRCH;
    }

    // uprobe 구조체 할당
    uprobe = kzalloc(sizeof(struct uprobe), GFP_KERNEL);
    if (!uprobe) {
        printk(KERN_ERR "ULP: Failed to allocate uprobe\n");
        return -ENOMEM;
    }

    // uprobe 설정
    uprobe->handler = handler_pre;
    uprobe->offset = target_addr;
    uprobe->inode = target_task->mm->exe_file->f_inode;

    // uprobe 등록
    ret = uprobe_register(uprobe->inode, target_addr, uprobe);
    if (ret) {
        printk(KERN_ERR "ULP: Failed to register uprobe: %d\n", ret);
        kfree(uprobe);
        return ret;
    }

    // ftrace 등록 (디버깅용)
    ret = ftrace_set_filter_ip(&ulp_ftrace_ops, target_addr, 0, 0);
    if (ret) {
        printk(KERN_ERR "ULP: Failed to set ftrace filter: %d\n", ret);
        uprobe_unregister(uprobe->inode, target_addr, uprobe);
        kfree(uprobe);
        return ret;
    }

    ret = register_ftrace_function(&ulp_ftrace_ops);
    if (ret) {
        printk(KERN_ERR "ULP: Failed to register ftrace: %d\n", ret);
        uprobe_unregister(uprobe->inode, target_addr, uprobe);
        kfree(uprobe);
        return ret;
    }

    // misc device 등록
    ret = misc_register(&ulp_dev);
    if (ret) {
        printk(KERN_ERR "ULP: Failed to register misc device: %d\n", ret);
        unregister_ftrace_function(&ulp_ftrace_ops);
        uprobe_unregister(uprobe->inode, target_addr, uprobe);
        kfree(uprobe);
        return ret;
    }

    printk(KERN_INFO "ULP: Uprobe registered at 0x%lx for PID %d\n",
           target_addr, target_pid);
    return 0;
}

// 모듈 종료
static void __exit ulp_exit(void)
{
    if (uprobe) {
        uprobe_unregister(uprobe->inode, target_addr, uprobe);
        kfree(uprobe);
    }
    unregister_ftrace_function(&ulp_ftrace_ops);
    misc_deregister(&ulp_dev);
    printk(KERN_INFO "ULP: Unloaded\n");
}

module_init(ulp_init);
module_exit(ulp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grok");
MODULE_DESCRIPTION("Userspace livepatch using ftrace and uprobe");
