#pragma once
#include "Job.h"
#include "DBJob.h"
#include "LockQueue.h"
#include "MPSCQueue.h"
#include "JobTimer.h"

class JobQueue : public enable_shared_from_this<JobQueue>
{
public:
	template<typename T, typename Ret, typename... Args>
	void DoDBAsync(Ret(T::* memFunc)(DBConnection*, Args...), Args... args)
	{
		shared_ptr<T> owner = static_pointer_cast<T>(shared_from_this());

		using TupleType = std::tuple<std::decay_t<Args>...>;
		TupleType tup = std::make_tuple(std::move(args)...);

		auto callback = [owner, memFunc, tup = std::move(tup)](DBConnection* dbConn) mutable
			{
				std::apply([&](auto&&... unpackedArgs) {
					(owner.get()->*memFunc)(dbConn, std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
					}, tup);
			};

		PushDBJob(ObjectPool<DBJob>::MakeShared(std::move(callback)));
	}

	void DoTimer(uint64 tickAfter, CallbackType&& callback)
	{
		JobRef job = make_shared<Job>(std::move(callback));
		GJobTimer->Reserve(tickAfter, shared_from_this(), job);
	}

	template<typename T, typename Ret, typename... Args>
	void DoTimer(uint64 tickAfter, Ret(T::* memFunc)(Args...), Args... args)
	{
		shared_ptr<T> owner = static_pointer_cast<T>(shared_from_this());
		JobRef job = make_shared<Job>(owner, memFunc, std::forward<Args>(args)...);
		GJobTimer->Reserve(tickAfter, shared_from_this(), job);
	}

	void					ClearJobs() { _jobs.Clear(); }

public:
	void					PushDBJob(DBJobRef dbJob);
	void					ExecuteDBJob(DBConnection* dbConn);

protected:
	LockQueue<DBJobRef>		_dbJobs;
	atomic<int32>			_jobCount = 0;
};

