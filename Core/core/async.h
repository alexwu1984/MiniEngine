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

#pragma once
#include "core/inc.h"

// This is a poor's man multithreaded lib. This is how it works:
//
// Each task is invoked by the app thread using the Async class, this class executes the shader compilation in a new thread.
// To prevent context switches we need to limit the number of running threads to the number of cores. 
// This is done by a global counter that keeps track of the number of running threads. 
// This counter gets incremented when the thread is running a task and decremented when it finishes.
// It is also decremented when a thread is put into Wait mode and incremented when a thread is signaled AND there is a core available to resume the thread.
// If all cores are busy the app thread is put to Wait to prevent it from spawning more threads.


namespace core
{
	class Sync;
	class Async;

	class AsyncPool
	{
		std::vector<Async*> m_pool;
	public:
		~AsyncPool();

		static AsyncPool& GetInstance();

		void Flush();
		void AddAsyncTask(std::function<void()> job, Sync* pSync = NULL);
	};

	void ExecAsync(std::function<void()> job);
}

