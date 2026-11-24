#include "pch.h"
#include "JobQueue.h"
#include "GlobalQueue.h"

void JobQueue::PushDBJob(DBJobRef dbJob)
{
	const int32 prevCount = _jobCount.fetch_add(1);
	_dbJobs.Push(dbJob);

	if (prevCount == 0 && LCurrentJobQueue == nullptr)
		GGlobalDBQueue->Push(shared_from_this());
}

void JobQueue::ExecuteDBJob(DBConnection* dbConn)
{
	if (LCurrentJobQueue != nullptr)
		return;

	LCurrentJobQueue = this;

	while (true)
	{
		vector<DBJobRef> jobs;
		_dbJobs.PopAll(OUT jobs);

		const int32 jobCount = static_cast<int32>(jobs.size());
		for (int32 i = 0; i < jobCount; i++)
			jobs[i]->Execute(dbConn); // DBConnection РќДо

		if (_jobCount.fetch_sub(jobCount) == jobCount)
		{
			LCurrentJobQueue = nullptr;
			return;
		}

		const uint64 now = ::GetTickCount64();
		if (now >= LEndTickCount)
		{
			LCurrentJobQueue = nullptr;
			GGlobalDBQueue->Push(shared_from_this());
			break;
		}
	}
}
