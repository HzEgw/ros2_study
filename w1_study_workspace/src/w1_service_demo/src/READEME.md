先说说今天写代码的不足
1.很多名字还是不够规范
2.书写代码的时间太长，两段用了1个小时左右

不过，我感觉昨天的学习把我之前的学习无法思考的地方已经慢慢的接上了
回顾一下昨天的概念
我的节点对象，例如 auto node = std::make_shared<ADDTwoService>("w1_service"); 创造的对象当中，我的所有的对象内部确实内存都存在于new区，这是为了我的节点对象的生死让我自己决定。是我对象内部保存的只是我的publish的句柄。那么真正的publish保存在哪里呢
首先dds传输是要依靠c语言编写的，这就意味着我的dds传输通道的“类”publish应该都是结构体，不是c+的类
那我写的智能指针的目的就是为了接管真实的publish结构体实例化的内存
那么我的publish实例化到底存在哪里
这或许涉及到某一个高级知识，但是我猜测ros2这个工程化程度极高的肯定是设置了一个专门的内存区域来统一放置这些内存
那么这样也就能够说明白一部分服务的流程了
 Promise promise;
    auto shared_future = promise.get_future().share();
    auto req_id = async_send_request_impl(
      *request,
      std::make_tuple(
        CallbackType{std::forward<CallbackT>(cb)},
        shared_future,
        std::move(promise)));
    return SharedFutureAndRequestId{std::move(shared_future), req_id};
    和async_send_request_impl(const Request & request, CallbackInfoVariant value)
  {
    int64_t sequence_number;
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    rcl_ret_t ret = rcl_send_request(get_client_handle().get(), &request, &sequence_number);
    if (RCL_RET_OK != ret) {
      rclcpp::exceptions::throw_from_rcl_error(ret, "failed to send request");
    }
    pending_requests_.try_emplace(
      sequence_number,
      std::make_pair(std::chrono::system_clock::now(), std::move(value)));
    return sequence_number;
  }
    我注意到这两段代码我有很多都看不明白，比如    return SharedFutureAndRequestId{std::move(shared_future), req_id};这一段的作用是什么
    我明白了，原来这里是一段创造 SharedFutureAndRequestId的对象，这里的std::move(shared_future)直接给了我的对象内部的属性，从而保证我的promis存活，的那时正如我的源码add_clinet->async_send_request(request,[this](const rclcpp::Client<ADDInterface>::SharedFuture future)->void{
                 auto get_respond=future.get();
                 RCLCPP_INFO(this->get_logger(), "收到响应:sum = %ld", get_respond->sum);
            });实际上我没有接受SharedFutureAndRequestId这个实例化，那这个实例化终究会被销毁？
            明白了，在容器   std::make_tuple(
        CallbackType{std::forward<CallbackT>(cb)},
        shared_future,
        std::move(promise)));实际上还保存了一份shared_future，这就保证了我的promis的存活，`promise.get_future()`：从 promise 身上拿出一个`future`
        std::move(promise)这一步是完成所有权转移，让我的ros调用这个包的时候可以更改promis
        那其实就很明白了，一问一答其实做的事情就是用完tuple之后就被删掉了，我的回调函数就没用了，自然是一问一答复，这也是设置在定时器里面的原因，每一次都会设置一个新的出来。
        其实这也能解释为什么我的服务回调参数是要用智能指针？但是我的订阅者直接传入结构体就行？

        至于之前说的那个特别的内存区域，其实应该就是dds的缓冲区。
