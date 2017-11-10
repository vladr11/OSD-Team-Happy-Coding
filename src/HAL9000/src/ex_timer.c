#include "HAL9000.h"
#include "ex_timer.h"
#include "iomu.h"
#include "thread_internal.h"

//Project_1
typedef struct _EX_TIMER_SYSTEM_DATA
{
	LOCK AllTimersLock;

	_Guarded_by_(AllTimersLock)
	LIST_ENTRY AllTimers;

}EX_TIMER_SYSTEM_DATA, *PEX_TIMER_SYSTEM_DATA;

EX_TIMER_SYSTEM_DATA m_ExTimerSystemData;

void
_No_competing_thread_
TimerSystemPreinit(
	void
)
{
	memzero(&m_ExTimerSystemData, sizeof(EX_TIMER_SYSTEM_DATA));

	InitializeListHead(&m_ExTimerSystemData.AllTimers);
	LockInit(&m_ExTimerSystemData.AllTimersLock);
}


STATUS
ExTimerInit(
	OUT     PEX_TIMER       Timer,
	IN      EX_TIMER_TYPE   Type,
	IN      QWORD           Time
)
{
	LOGL("ExTimerInit() called\n");
	STATUS status;

	if (NULL == Timer)
	{
		return STATUS_INVALID_PARAMETER1;
	}

	if (Type > ExTimerTypeMax)
	{
		return STATUS_INVALID_PARAMETER2;
	}

	//Project_1
	if (Time <= 0)
	{
		return STATUS_INVALID_PARAMETER3;
	}

    status = STATUS_SUCCESS;

    memzero(Timer, sizeof(EX_TIMER));

    Timer->Type = Type;
    if (Timer->Type != ExTimerTypeAbsolute)
    {
        // relative time
        // if the time trigger time has already passed the timer will
        // be signaled after the first scheduler tick
        Timer->TriggerTimeUs = IomuGetSystemTimeUs() + Time;
        Timer->ReloadTimeUs = Time;
    }
    else
    {
        // absolute
        Timer->TriggerTimeUs = Time;
    }

	//Project_1
	ExEventInit(&Timer->Event, ExEventTypeNotification, FALSE);
	LOGL("Timer event initialized\n");

    return status;
}

void
ExTimerStart(
    IN      PEX_TIMER       Timer
    )
{
	INTR_STATE dummyState;

    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    Timer->TimerStarted = TRUE;

	//Project_1
	LockAcquire(&m_ExTimerSystemData.AllTimersLock, &dummyState);
	InsertHeadList(&m_ExTimerSystemData.AllTimers, &Timer->TimerListEntry);
	LockRelease(&m_ExTimerSystemData.AllTimersLock, dummyState);
}

void
ExTimerStop(
    IN      PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    Timer->TimerStarted = FALSE;

	//Project_1
	ExEventSignal(&(Timer->Event));
	ExEventClearSignal(&(Timer->Event));
}

//Project_1
void TimerTick()
{	
	INTR_STATE dummyState;
	INTR_STATE oldState;

	oldState = CpuIntrDisable();

	PLIST_ENTRY pEntry;
	LIST_ITERATOR iterator;
	LockAcquire(&m_ExTimerSystemData.AllTimersLock, &dummyState);
	ListIteratorInit(&m_ExTimerSystemData.AllTimers, &iterator);
	while ((pEntry = ListIteratorNext(&iterator))!=NULL)
	{
		PEX_TIMER Timer = CONTAINING_RECORD(pEntry, EX_TIMER, TimerListEntry);
		if (IomuGetSystemTimeUs() >= Timer->TriggerTimeUs && Timer->TimerStarted)
		{
			
			if (Timer->Type != ExTimerTypeRelativePeriodic)
			{
				//ExTimerUninit apeleaza ExTimerStop care apeleaza ExSignalEvent
				ExTimerUninit(Timer);
				RemoveEntryList(pEntry);
			}
			else 
			{
				Timer->TriggerTimeUs = IomuGetSystemTimeUs() + Timer->ReloadTimeUs;
				ExEventSignal(&(Timer->Event));
				ExEventClearSignal(&(Timer->Event));
			}
		}
	}
	LockRelease(&m_ExTimerSystemData.AllTimersLock, dummyState);

	
	CpuIntrSetState(oldState);
}



void
ExTimerWait(
    INOUT   PEX_TIMER       Timer
    )
{
	LOGL("ExTimerWait() called\n");

    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

	//Project_1
	if (Timer->TimerStarted)
	{
		ExEventWaitForSignal(&Timer->Event);
	}
}

void
ExTimerUninit(
    INOUT   PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    ExTimerStop(Timer);

    Timer->TimerUninited = TRUE;
}

INT64
ExTimerCompareTimers(
    IN      PEX_TIMER     FirstElem,
    IN      PEX_TIMER     SecondElem
)
{
    return FirstElem->TriggerTimeUs - SecondElem->TriggerTimeUs;
}