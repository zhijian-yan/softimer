# softimer
跨平台的基于时间差比较的软件定时器库
# 特性
- 利用无符号数的溢出绕回和有符号数差值比较避免了计数器溢出的问题
- 使用全局递增计数器作为时间基准
- 自动管理定时器链表，包括定时器的增加移除和到期时间的升序排序
- 定时器重载时基于当前时刻计算目标时刻，无误差累积
- 采用面向对象的思想进行封装
- 硬件无关，跨平台，无需第三方依赖

# 移植
- **softimer**的代码为C语言设计，支持C++环境，移植无需其他步骤，直接将源代码添加到项目中即可
- 对象的创建默认使用C语言标准库的`malloc`函数，如有特殊的内存分配需求，可以将头文件中`stim_malloc`和`stim_free`宏替换为项目中所使用的内存分配函数

# 注意事项
- 不能在中断中调用`stim_delete`函数，会有`Use-After-Free`风险
- `stim_delete`函数仅释放内存，不会设置指针为NULL，请手动设置
- 可以在`softimer_cb`中调用`stim_delete`函数

# 使用
## 第一步 设置全局递增计数器
在硬件定时器的回调中调用`stim_systick_inc`函数并按照你的需要设置硬件定时器的频率，这将为软件定时器提供时间基准
```c
void hardtimer_callback(void) // 硬件定时器回调
{
    stim_systick_inc();
}
```
## 第二步 创建定时器实例
创建一个软件定时器的实例并设置它的回调函数
```c
void softimer_cb(stim_handle_t timer, void *user_data)
{
    // 添加你的代码
}

stim_handle_t timer = stim_create(1000, softimer_cb, NULL);
```
## 第三步 启动定时器
调用`stim_start`函数启动定时器，它会将你创建的定时器加入到全局链表中并按照到期时间进行升序排序
```c
stim_start(timer);
```
## 第四步 周期性调用定时器处理程序
最后需要周期性调用定时器的到期处理程序
```c
while(1)
{
    stim_handler();
}
```
## 第五步 开始享受softimer吧！
