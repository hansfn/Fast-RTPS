/*************************************************************************
 * Copyright (c) 2013 eProsima. All rights reserved.
 *
 * This generated file is licensed to you under the terms described in the
 * fastrtps_LICENSE file included in this fastrtps distribution.
 *
 *************************************************************************
 * 
 * @file SimpleSubscriber.h
 * This header file contains the declaration of the subscriber functions.
 *
 * This file was generated by the tool fastrtpsgen.
 */


#ifndef _Simple_SUBSCRIBER_H_
#define _Simple_SUBSCRIBER_H_

#include "fastrtps/rtps_all.h"

class SimpleSubscriber 
{
public:
	SimpleSubscriber();
	virtual ~SimpleSubscriber();
	bool init();
	void run();
private:
	RTPSParticipant *mp_RTPSParticipant;
	Subscriber *mp_subscriber;
	
	class SubListener : public SubscriberListener
	{
	public:
		SubListener() : n_matched(0){};
		~SubListener(){};
		void onSubscriptionMatched(MatchingInfo info);
		void onNewDataMessage();
		SampleInfo_t m_info;
		Subscriber* mp_sub;
		int n_matched;
	} m_listener;
};

#endif // _Simple_SUBSCRIBER_H_