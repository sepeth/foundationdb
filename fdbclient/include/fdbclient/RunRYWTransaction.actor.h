/*
 * RunRYWTransaction.actor.h
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

#pragma once

// When actually compiled (NO_INTELLISENSE), include the generated version of this file.  In intellisense use the source
// version.
#if defined(NO_INTELLISENSE) && !defined(FDBCLIENT_RUNRYWTRANSACTION_ACTOR_G_H)
#define FDBCLIENT_RUNRYWTRANSACTION_ACTOR_G_H
#include "fdbclient/RunRYWTransaction.actor.g.h"
#elif !defined(FDBCLIENT_RUNRYWTRANSACTION_ACTOR_H)
#define FDBCLIENT_RUNRYWTRANSACTION_ACTOR_H

#include <utility>

#include "flow/flow.h"
#include "fdbclient/RunTransaction.actor.h"
#include "fdbclient/ReadYourWrites.h"
#include "flow/actorcompiler.h" // This must be the last #include.

// Runs a RYW transaction in a retry loop on the given Database.
//
// Takes a function func that accepts a Reference<ReadYourWritesTransaction> as a parameter and returns a non-Void
// Future. This function is run inside the transaction, and when the transaction is successfully committed the result of
// the function is returned.
//
// The supplied function should be idempotent. Otherwise, outcome of this function will depend on how many times the
// transaction is retried.
ACTOR template <class Function>
Future<decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue())> runRYWTransaction(
    Database cx,
    Function func) {
	state Reference<ReadYourWritesTransaction> tr(new ReadYourWritesTransaction(cx));
	loop {
		try {
			state decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue()) result =
			    wait(func(tr));
			wait(tr->commit());
			return result;
		} catch (Error& e) {
			wait(tr->onError(e));
		}
	}
}

// Debug version of runRYWTransaction. It logs the function name and the committed version of the transaction.
// Note the function name is required, e.g., taskFunc->getName() for TaskFuncBase.
ACTOR template <class Function>
Future<decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue())>
runRYWTransactionDebug(Database cx, StringRef name, Function func) {
	state Reference<ReadYourWritesTransaction> tr(new ReadYourWritesTransaction(cx));
	loop {
		try {
			// func should be idempodent; otherwise, retry will get undefined result
			state decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue()) result =
			    wait(func(tr));
			wait(tr->commit());
			TraceEvent("DebugRunRYWTransaction")
			    .detail("Function", name)
			    .detail("CommitVersion", tr->getCommittedVersion());
			return result;
		} catch (Error& e) {
			wait(tr->onError(e));
		}
	}
}

// Runs a RYW transaction in a retry loop on the given Database.
//
// Takes a function func that accepts a Reference<ReadYourWritesTransaction> as a parameter and returns a Void
// Future. This function is run inside the transaction.
//
// The supplied function should be idempotent. Otherwise, outcome of this function will depend on how many times the
// transaction is retried.
ACTOR template <class Function>
Future<Void> runRYWTransactionVoid(Database cx, Function func) {
	state Reference<ReadYourWritesTransaction> tr(new ReadYourWritesTransaction(cx));
	loop {
		try {
			wait(func(tr));
			wait(tr->commit());
			return Void();
		} catch (Error& e) {
			wait(tr->onError(e));
		}
	}
}

ACTOR template <class Function>
Future<decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue())>
runRYWTransactionFailIfLocked(Database cx, Function func) {
	state Reference<ReadYourWritesTransaction> tr(new ReadYourWritesTransaction(cx));
	loop {
		try {
			state decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue()) result =
			    wait(func(tr));
			wait(tr->commit());
			return result;
		} catch (Error& e) {
			if (e.code() == error_code_database_locked)
				throw;
			wait(tr->onError(e));
		}
	}
}

ACTOR template <class Function>
Future<decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue())> runRYWTransactionNoRetry(
    Database cx,
    Function func) {
	state Reference<ReadYourWritesTransaction> tr(new ReadYourWritesTransaction(cx));
	state decltype(std::declval<Function>()(Reference<ReadYourWritesTransaction>()).getValue()) result = wait(func(tr));
	wait(tr->commit());
	return result;
}

#include "flow/unactorcompiler.h"
#endif
