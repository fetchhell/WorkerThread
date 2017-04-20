#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <future>
#include <memory>
#include <functional>
#include <condition_variable>
#include <string>
#include <algorithm>

class ThreadPool
{
	std::vector<std::thread> worker_threads;
	std::queue<std::function<void()> > tasks_queue;

	std::mutex mutex_lock;
	std::condition_variable condition_lock;

	bool enabled;

public:
	ThreadPool(const size_t& worker_threads_amount);

	template<class _FUNC, class... _ARGS>
	auto enqueue(_FUNC&& _func_ptr, _ARGS&&... _args)->std::future<typename std::result_of<_FUNC(_ARGS...)>::type>;

	~ThreadPool();
}; // END : ThreadPool class definition


ThreadPool::ThreadPool(const size_t& worker_threads_amount) : enabled(true)
{
	for (size_t i = 0; i < worker_threads_amount; ++i)
	{
		worker_threads.emplace_back([this]
		{
			while (enabled)
			{
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> locker(mutex_lock);
					condition_lock.wait(locker, [this] { return !tasks_queue.empty() || !enabled; });
					if (!enabled && tasks_queue.empty()) return;
					task = std::move(tasks_queue.front());
					tasks_queue.pop(); // pop a front task from the queue
				}
				task(); // execute task
			}
		});
	}
} // END ctor : ThreadPool::ThreadPool

ThreadPool::~ThreadPool()
{
	std::unique_lock<std::mutex> lock(mutex_lock);

	enabled = false;
	condition_lock.notify_all();

	for (auto& worker_thread : worker_threads)
		worker_thread.join();
} // END dtor : ThreadPool::~ThreadPool

template<class _FUNC, class... _ARGS>
auto ThreadPool::enqueue(_FUNC&& _func_ptr, _ARGS&&... _args)->std::future<typename std::result_of<_FUNC(_ARGS...)>::type>
{
	using return_type = typename std::result_of<_FUNC(_ARGS...)>::type;
	auto task = std::make_shared<std::packaged_task<return_type()> >(std::bind(std::forward<_FUNC>(_func_ptr), std::forward<_ARGS>(_args)...));

	std::future<return_type> output_result = task->get_future();
	{
		std::unique_lock<std::mutex> lock(mutex_lock);
		tasks_queue.emplace([task]() { (*task)(); });
	}

	condition_lock.notify_one();
	return output_result;
} // END : enqueue

template<typename _MAGIC_TYPE>
auto performAction(_MAGIC_TYPE qualifier)->decltype(qualifier)
{
	uint32_t thread_id = *(reinterpret_cast<uint32_t*>(&std::this_thread::get_id()));
	thread_id = thread_id & 0xff << 0x08 >> 0x20 ^ 0xfe;

	std::rotate(qualifier.rbegin(), qualifier.rbegin() + thread_id % qualifier.size(), qualifier.rend());

	for (auto& item : qualifier)
		item += qualifier.size();

	return qualifier;
} // END : perform action

int main(int argc, char* argv[])
{
	ThreadPool pool(4);

	std::string encoded_key(argv[1]);
	uint32_t split_interval = atoi(argv[2]);

	std::vector<std::string> encoded_key_splitted;
	for (size_t i = 0; i < encoded_key.size(); i += split_interval)
	{
		auto divide_key = [=]() { return encoded_key.substr(i, split_interval); };
		encoded_key_splitted.emplace_back(divide_key());
	}

	std::vector<std::future<std::string> > decoded_key;
	for (size_t i = 0; i < encoded_key_splitted.size(); ++i)
	{
		decoded_key.emplace_back(pool.enqueue([=] { return performAction(encoded_key_splitted[i]); }));
	}

	for (auto&& part_of_key : decoded_key)
		std::cout << "The key is :" << part_of_key.get();
	std::cout << std::endl;

	return EXIT_SUCCESS;
} // END : main
