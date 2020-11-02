#ifndef	_LOG_PRINTER_H_
#define _LOG_PRINTER_H_

#include "util.h"

class LogPrinter
{
public:
	LogPrinter()
	{
		m_nOnceMaxCount = 4;
		m_nOnceCount = 0;
		m_nTimeInterval = 60000;
		m_nLastPrintTime = 0;
	}

	LogPrinter(int timeInterval, int onceMaxCount)
	{
		m_nOnceMaxCount = onceMaxCount;
		m_nOnceCount = 0;
		m_nTimeInterval = timeInterval;
		m_nLastPrintTime = 0;
	}

	~LogPrinter() {}

	bool CanPrint()
	{
		int timeNow = OSGetTimeNow();
		if ((timeNow - m_nLastPrintTime) < m_nTimeInterval)
		{
			m_nOnceCount++;
			if (m_nOnceCount <= m_nOnceMaxCount)
			{
				m_nLastPrintTime = timeNow;
				return true;
			}
			else
			{				
				return false;
			}
		}
		else
		{
			m_nOnceCount = 0;
			m_nOnceCount++;
			m_nLastPrintTime = timeNow;
			return true;
		}
	}

private:
	int m_nOnceMaxCount;
	int m_nOnceCount;
	int m_nTimeInterval;
	int m_nLastPrintTime;
};

#endif