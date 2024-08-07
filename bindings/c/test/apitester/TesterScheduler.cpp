/*
 * TesterScheduler.cpp
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2024 Apple Inc. and the FoundationDB project authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "TesterScheduler.h"
#include "TesterUtil.h"

#include <boost/asio/detail/chrono.hpp>
#include <memory>
#include <thread>
#include <boost/asio.hpp>

using namespace boost::asio;

namespace FdbApiTester {

const TTaskFct NO_OP_TASK = []() {};

class AsioTimer : public ITimer {
public:
	AsioTimer(io_context& io_ctx, chrono::steady_clock::duration time) : impl(io_ctx, time) {}

	void cancel() override { impl.cancel(); }

	boost::asio::steady_timer impl;
};

class AsioScheduler : public IScheduler {
public:
	AsioScheduler(int numThreads) : numThreads(numThreads) {}

	void start() override {
		work = require(io_ctx.get_executor(), execution::outstanding_work.tracked);
		for (int i = 0; i < numThreads; i++) {
			threads.emplace_back([this]() { io_ctx.run(); });
		}
	}

	void schedule(TTaskFct task) override { post(io_ctx, task); }

	std::unique_ptr<ITimer> scheduleWithDelay(int delayMs, TTaskFct task) override {
		auto timer = std::make_unique<AsioTimer>(io_ctx, boost::asio::chrono::milliseconds(delayMs));
		timer->impl.async_wait([task](const boost::system::error_code& e) {
			if (!e) {
				task();
			}
		});
		return timer;
	}

	void stop() override { work = any_io_executor(); }

	void join() override {
		for (auto& th : threads) {
			th.join();
		}
	}

private:
	int numThreads;
	std::vector<std::thread> threads;
	io_context io_ctx;
	any_io_executor work;
};

std::unique_ptr<IScheduler> createScheduler(int numThreads) {
	ASSERT(numThreads > 0 && numThreads <= 1000);
	return std::make_unique<AsioScheduler>(numThreads);
}

} // namespace FdbApiTester