#pragma once
#include <QtWidgets>

class thread_info_event : public QEvent
{
	Q_OBJECT
	private:
		std::pair<uint16_t, uint16_t> semaphor_info;
	public:
		thread_info_event() : QEvent((Type)(QEvent::User + 100))
		{
			semaphor_info.first = 0;
			semaphor_info.second = 0;
		}
		thread_info_event(uint16_t s_id, uint16_t q) : QEvent((Type)(QEvent::User + 100))
		{
			semaphor_info.first = s_id;
			semaphor_info.second = q;
		}
		std::pair<uint16_t, uint16_t> info()
		{
			return semaphor_info;
		}
};
