// AMD Cauldron code
// 
// Copyright(c) 2017 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "async.h"

namespace core
{
	class Sync
	{
		int m_count = 0;
		std::mutex m_mutex;
		std::condition_variable condition;
	public:
		int Inc()
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_count++;
			return m_count;
		}

		int Dec()
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_count--;
			if (m_count == 0)
				condition.notify_all();
			return m_count;
		}

		int Get()
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			return m_count;
		}

		void Reset()
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_count = 0;
			condition.notify_all();
		}

		void Wait()
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			while (m_count != 0)
				condition.wait(lock);
		}

	};

	class Async
	{
		static int s_activeThreads;
		static int s_maxThreads;
		static std::mutex s_mutex;
		static std::condition_variable s_condition;
		static bool s_bExiting;

		std::function<void()> m_job;
		Sync* m_pSync;
		std::thread* m_pThread;

	public:
		Async(std::function<void()> job, Sync* pSync = NULL);
		~Async();
		static void Wait(Sync* pSync);
	};

	Async::Async(std::function<void()> job, Sync* pSync) :
		m_job{ job },
		m_pSync{ pSync }
	{
		if (m_pSync)
			m_pSync->Inc();

		{
			std::unique_lock<std::mutex> lock(s_mutex);

			while (s_activeThreads >= s_maxThreads)
			{
				s_condition.wait(lock);
			}

			s_activeThreads++;
		}

		m_pThread = new std::thread([this]()
			{
				m_job();

		{
			std::lock_guard<std::mutex> lock(s_mutex);
			s_activeThreads--;
		}

		s_condition.notify_one();

		if (m_pSync)
			m_pSync->Dec();
			});
	}

	Async::~Async()
	{
		m_pThread->join();
		delete m_pThread;
	}

	void Async::Wait(Sync* pSync)
	{
		if (pSync->Get() == 0)
			return;

		{
			std::lock_guard <std::mutex> lock(s_mutex);
			s_activeThreads--;
		}

		s_condition.notify_one();

		pSync->Wait();

		{
			std::unique_lock<std::mutex> lock(s_mutex);

			s_condition.wait(lock, []
				{
					return s_bExiting || (s_activeThreads < s_maxThreads);
				});

			s_activeThreads++;
		}
	}

	//
	// Basic async pool
	//

	AsyncPool::~AsyncPool()
	{
		Flush();
	}

	AsyncPool& AsyncPool::GetInstance()
	{
		static AsyncPool _Pool;
		return _Pool;
	}

	void AsyncPool::Flush()
	{
		for (int i = 0; i < m_pool.size(); i++)
			delete m_pool[i];
		m_pool.clear();
	}

	void AsyncPool::AddAsyncTask(std::function<void()> job, Sync* pSync)
	{
		m_pool.push_back(new Async(job, pSync));
	}

	//
	// ExecAsyncIfThereIsAPool, will use async if there is a pool, otherwise will run the task synchronously
	void ExecAsync(std::function<void()> job)
	{
		AsyncPool::GetInstance().AddAsyncTask(job);
	}

	//
	// Some static functions
	//
	int Async::s_activeThreads = 0;
	std::mutex Async::s_mutex;
	std::condition_variable Async::s_condition;
	bool Async::s_bExiting = false;
	int Async::s_maxThreads = std::thread::hardware_concurrency();
}