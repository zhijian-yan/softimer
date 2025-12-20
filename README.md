# softimer——跨平台软件定时器库
## 特性
- 利用无符号数的溢出绕回和有符号数差值比较避免了计数器溢出的问题
- 定时器重载时基于当前时刻计算目标时刻，无误差累积
- 自动管理定时器链表并按照到期时间升序排序
- 处理程序每次仅检查链表表头的定时器，同时到期的定时器按照链表排列顺序进行检查
- 软件与硬件解耦，通过回调函数关联硬件实现跨平台
## 移植
- **softimer**为纯C语言设计，将**softimer.c**和**softimer.h**添加到项目中即可
- 定时器实例的创建默认使用C语言标准库的`malloc`函数，如有特殊的内存分配需求，可以将头文件中`stim_malloc`和`stim_free`宏替换为项目中所使用的内存分配函数
## 注意事项
- 不能在中断中调用`stim_delete`函数，会有`Use-After-Free`风险
- 可以在定时器回调中调用`stim_delete`函数
- `stim_delete`函数仅释放内存，不会设置句柄为NULL，请手动设置
## 使用步骤
### 第一步 设置全局递增计数器
```c
void timer_callback(void) // 定时器回调
{
    stim_systick_inc();
}
```
在硬件定时器的回调中调用`stim_systick_inc`函数，这将为软件定时器提供时间基准
### 第二步 创建定时器实例并设置定时器回调
```c
void stim_cb(stim_handle_t timer, void *user_data)
{
    swtich((int)timer)
    {
        case (int)timer1:printf("timer1 count:%u\r\n", stim_get_count(timer));break;
        case (int)timer2:printf("timer2 count:%u\r\n", stim_get_count(timer));break;
    }
}

stim_handle_t timer1 = stim_create(100, stim_cb, NULL);
stim_handle_t timer2 = stim_create(1000, stim_cb, NULL);
```
多个定时器可以共用一个回调函数，通过`stim_handle_t timer`参数区分，也可以使用`user_data`参数自定义定时器编号进行区分
```c
void stim_cb(stim_handle_t timer, void *user_data)
{
    swtich((int)user_data)
    {
        case 1:printf("timer1 count:%u\r\n", stim_get_count(timer));break;
        case 2:printf("timer2 count:%u\r\n", stim_get_count(timer));break;
    }
}

stim_create(100, stim_cb, (void*)1);
stim_create(1000, stim_cb, (void*)2);
```
### 第三步 启动定时器
```c
stim_start(timer);
```
`stim_start`函数会将定时器添加到全局链表中并按照到期时间进行升序排序<br>
该函数执行过程中会通过`flag_critical`标志禁用`stim_handler`函数功能，这可能导致定时器丢时，但影响非常小，如对精度有要求建议在启动所有定时器后再周期性执行`stim_handler`函数
### 第四步 周期性调用处理程序
在硬件定时器回调中调用
```c
void timer_callback(void) // 硬件定时器回调
{
    stim_handler();
    stim_systick_inc();
}
```
在循环中调用
```c
while(1)
{
    stim_handler();
}
```
可以选择在循环和硬件定时器回调中周期性调用处理程序，在定时器回调中调用可以最大化软件定时器的精度
### 第五步 停止定时器和资源释放
```c
stim_stop(timer);
```
`stim_stop`函数会将定时器从全局定时器链表中移除<br>
该函数执行过程中会通过`flag_critical`标志禁用`stim_handler`函数功能，这可能导致定时器丢时，但影响非常小
```c
stim_delete(timer);
```
`stim_delete`函数删除定时器实例并释放所有资源，只有没有启动的定时器才能删除
### 第六步 开始享受softimer吧！
### 完整代码
```c
void timer_callback(void) // 硬件定时器回调
{
    stim_systick_inc();
}

void stim_cb(stim_handle_t timer, void *user_data)
{
    swtich((int)user_data)
    {
        case 1:printf("timer1 count:%u\r\n", stim_get_count(timer));break;
        case 2:printf("timer2 count:%u\r\n", stim_get_count(timer));break;
    }
}

void timer_init(void)
{
    // (添加硬件定时器的初始化代码)
    stim_startstim_create(100, stim_cb, (void*)1));
    stim_startstim_create(1000, stim_cb, (void*)2));
}

int main()
{
    timer_init();
    while(1)
    {
        stim_handler();
    }
}
```
