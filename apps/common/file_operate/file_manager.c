#include "file_manager.h"
#include "app_config.h"

FILE *file_manager_select(struct __dev *dev, struct vfscan *fs, int sel_mode, int arg, struct __scan_callback *callback)
{
    FILE *_file = NULL;
    //clock_add_set(SCAN_DISK_CLK);
    if (callback && callback->enter) {
        callback->enter(dev);//扫描前处理， 可以在注册的回调里提高系统时钟等处理
    }
    _file = fselect(fs, sel_mode, arg);
    //clock_remove_set(SCAN_DISK_CLK);
    if (callback && callback->exit) {
        callback->exit(dev);//扫描后处理， 可以在注册的回调里还原到enter前的状态
    }
    return _file;
}


