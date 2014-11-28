/*************************************************************************
 * Copyright (c) 2014 eProsima. All rights reserved.
 *
 * This copy of eProsima RTPS is licensed to you under the terms described in the
 * EPROSIMARTPS_LIBRARY_LICENSE file included in this distribution.
 *
 *************************************************************************/

/**
 * @file Participant.cpp
 *
 */

#include "eprosimartps/Participant.h"
#include "eprosimartps/pubsub/RTPSDomain.h"
#include "eprosimartps/writer/StatelessWriter.h"
#include "eprosimartps/reader/StatelessReader.h"
#include "eprosimartps/reader/StatefulReader.h"
#include "eprosimartps/writer/StatefulWriter.h"
#include "eprosimartps/reader/RTPSReader.h"
#include "eprosimartps/writer/RTPSWriter.h"

#include "eprosimartps/builtin/BuiltinProtocols.h"
#include "eprosimartps/builtin/discovery/participant/PDPSimple.h"

#include "eprosimartps/utils/RTPSLog.h"
#include "eprosimartps/utils/IPFinder.h"
#include "eprosimartps/utils/eClock.h"

namespace eprosima {
namespace rtps {

typedef std::vector<RTPSReader*>::iterator p_ReaderIterator;
typedef std::vector<RTPSWriter*>::iterator p_WriterIterator;


ParticipantImpl::ParticipantImpl(const ParticipantAttributes& PParam,
		const GuidPrefix_t& guidP,uint32_t ID,
		Participant* userP,
		ParticipantListener* plisten):
							m_defaultUnicastLocatorList(PParam.defaultUnicastLocatorList),
							m_defaultMulticastLocatorList(PParam.defaultMulticastLocatorList),
							m_participantName(PParam.name),
							m_guid(guidP,c_EntityId_Participant),
							#pragma warning(disable: 4355)
							m_send_thr(this),
							#pragma warning(disable: 4355)
							m_event_thr(this),
							mp_ResourceSemaphore(new boost::interprocess::interprocess_semaphore(0)),
							IdCounter(0),
							#pragma warning(disable: 4355)
							m_builtinProtocols(this),
							m_participantID(ID),
							m_send_socket_buffer_size(PParam.sendSocketBufferSize),
							m_listen_socket_buffer_size(PParam.listenSocketBufferSize),
							mp_participantListener(plisten),
							mp_userParticipant(userP)
{
	userP->mp_impl = this;
	m_userData = PParam.userData;

	Locator_t loc;
	loc.port = PParam.defaultSendPort;
	m_send_thr.initSend(loc);
	m_event_thr.init_thread();

	if(m_defaultUnicastLocatorList.empty() && m_defaultMulticastLocatorList.empty())
	{
		LocatorList_t myIP;
		IPFinder::getIPAddress(&myIP);
		std::stringstream ss;

		for(LocatorListIterator lit = myIP.begin();lit!=myIP.end();++lit)
		{
			lit->port=RTPSDomain::getPortBase()+
					RTPSDomain::getDomainIdGain()*PParam.builtin.domainId+
					RTPSDomain::getOffsetd3()+
					RTPSDomain::getParticipantIdGain()*ID;
			m_defaultUnicastLocatorList.push_back(*lit);
			ss << *lit << ";";
		}
		
		std::string auxstr = ss.str();
		pWarning("Participant created with NO default Unicast Locator List, adding Locators: "<<auxstr<<endl);
	}
	LocatorList_t defcopy = m_defaultUnicastLocatorList;
	m_defaultUnicastLocatorList.clear();
	for(LocatorListIterator lit = defcopy.begin();lit!=defcopy.end();++lit)
	{
		ListenResource* LR = new ListenResource(this);
		Locator_t loc = LR->init_thread(*lit,false,false);
		m_defaultUnicastLocatorList.push_back(loc);
		this->m_listenResourceList.push_back(LR);
	}
	defcopy = m_defaultMulticastLocatorList;
	m_defaultMulticastLocatorList.clear();
	for(LocatorListIterator lit = defcopy.begin();lit!=defcopy.end();++lit)
	{
		ListenResource* LR = new ListenResource(this);
		Locator_t loc = LR->init_thread(*lit,true,false);
		m_defaultMulticastLocatorList.push_back(loc);
		this->m_listenResourceList.push_back(LR);
	}


	pInfo("Participant \"" <<  m_participantName << "\" with guidPrefix: " <<m_guid.guidPrefix<< endl);


	m_builtin = PParam.builtin;
	

	//START BUILTIN PROTOCOLS
	m_builtinProtocols.initBuiltinProtocols(PParam.builtin,m_participantID);

}


ParticipantImpl::~ParticipantImpl()
{
	pDebugInfo("Participant destructor"<<endl;);
	//Destruct threads:
	for(std::vector<ListenResource*>::iterator it=m_listenResourceList.begin();
				it!=m_listenResourceList.end();++it)
			delete(*it);

	for(std::vector<RTPSReader*>::iterator it=m_userReaderList.begin();
			it!=m_userReaderList.end();++it)
		delete(*it);

	for(std::vector<RTPSWriter*>::iterator it=m_userWriterList.begin();
			it!=m_userWriterList.end();++it)
		delete(*it);

	delete(this->mp_ResourceSemaphore);

}

bool ParticipantImpl::existsEntityId(const EntityId_t& ent,EndpointKind_t kind) const 
{
	if(kind == WRITER)
	{
		for(std::vector<RTPSWriter*>::const_iterator it = m_userWriterList.begin();
			it!=m_userWriterList.end();++it)
		{
			if(ent == (*it)->getGuid().entityId)
				return true;
		}
	}
	else
	{
		for(std::vector<RTPSReader*>::const_iterator it = m_userReaderList.begin();
			it!=m_userReaderList.end();++it)
		{
			if(ent == (*it)->getGuid().entityId)
				return true;
		}
	}
	return false;
}


bool ParticipantImpl::createWriter(RTPSWriter** WriterOut,
		PublisherAttributes& param, uint32_t payload_size, bool isBuiltin,
		StateKind_t kind, TopicDataType* ptype, PublisherListener* inlisten,
		const EntityId_t& entityId)
{
	std::string type = (kind == STATELESS) ? "STATELESS" :"STATEFUL";
	pDebugInfo("Creating " << type << " Writer on topic: "<<param.topic.getTopicName()<<endl);
	EntityId_t entId;
	if(entityId== c_EntityId_Unknown)
	{
		if(param.topic.getTopicKind() == NO_KEY)
			entId.value[3] = 0x03;
		else if(param.topic.getTopicKind() == WITH_KEY)
			entId.value[3] = 0x02;
		uint32_t idnum;
		if(param.getEntityId()>0)
			idnum = param.getEntityId();
		else
		{
			IdCounter++;
			idnum = IdCounter;
		}
		
		octet* c = (octet*)&idnum;
		entId.value[2] = c[0];
		entId.value[1] = c[1];
		entId.value[0] = c[2];
		if(this->existsEntityId(entId,WRITER))
		{
			pError("A writer with the same entityId already exists in this participant"<<endl;);
			return false;
		}
	}
	else
	{
		entId = entityId;
	}
	RTPSWriter* SWriter = NULL;
	if(kind == STATELESS)
		SWriter = (RTPSWriter*)new StatelessWriter(param, m_guid.guidPrefix,entId,ptype);
	else if(kind == STATEFUL)
		SWriter = (RTPSWriter*)new StatefulWriter(param, m_guid.guidPrefix,entId,ptype);
	if(SWriter==NULL)
		return false;
	//If NO UNICAST
	SWriter->setListener(inlisten);
	SWriter->setQos(param.qos,true);
	SWriter->mp_send_thr = &this->m_send_thr;
	SWriter->mp_event_thr = &this->m_event_thr;
	if(kind == STATEFUL)
	{
		if(!assignEndpointListenResources((Endpoint*)SWriter,isBuiltin))
		{
			delete(SWriter);
			return false;
		}
	}
	m_allWriterList.push_back(SWriter);


	*WriterOut = SWriter;
	return true;
}

static EntityId_t TrustedWriter(const EntityId_t& reader)
{
	if(reader == c_EntityId_SPDPReader) return c_EntityId_SPDPWriter;
	if(reader == c_EntityId_SEDPPubReader) return c_EntityId_SEDPPubWriter;
	if(reader == c_EntityId_SEDPSubReader) return c_EntityId_SEDPSubWriter;
	if(reader == c_EntityId_ReaderLiveliness) return c_EntityId_WriterLiveliness;

	return c_EntityId_Unknown;
}

bool ParticipantImpl::createReader(RTPSReader** ReaderOut,
		SubscriberAttributes& param, uint32_t payload_size, bool isBuiltin,
		StateKind_t kind, TopicDataType* ptype, SubscriberListener* inlisten,
		const EntityId_t& entityId)
{
	std::string type = (kind == STATELESS) ? "STATELESS" :"STATEFUL";
	pDebugInfo("Creating " << type << " Reader on topic: "<<param.topic.getTopicName()<<endl);
	EntityId_t entId;
	if(entityId == c_EntityId_Unknown)
	{
		if(param.topic.getTopicKind() == NO_KEY)
			entId.value[3] = 0x04;
		else if(param.topic.getTopicKind() == WITH_KEY)
			entId.value[3] = 0x07;
		uint32_t idnum;
		if(param.getEntityId()>0)
			idnum = param.getEntityId();
		else
		{
			IdCounter++;
			idnum = IdCounter;
		}
		octet* c = (octet*)&idnum;
		entId.value[2] = c[0];
		entId.value[1] = c[1];
		entId.value[0] = c[2];
		if(this->existsEntityId(entId,READER))
		{
			pError("A reader with the same entityId already exists in this participant"<<endl;);
			return false;
		}
	}
	else
		entId = entityId;
	RTPSReader* SReader = NULL;
	if(kind == STATELESS)
		SReader = (RTPSReader*)new StatelessReader(param, m_guid.guidPrefix,entId,ptype);
	else if(kind == STATEFUL)
		SReader = (RTPSReader*)new StatefulReader(param, m_guid.guidPrefix,entId,ptype);
	if(SReader==NULL)
		return false;
	SReader->setListener(inlisten);
	SReader->setQos(param.qos,true);
	SReader->mp_send_thr = &this->m_send_thr;
	SReader->mp_event_thr = &this->m_event_thr;
	if(isBuiltin)
	{
		SReader->setTrustedWriter(TrustedWriter(SReader->getGuid().entityId));
	}
	if(!assignEndpointListenResources((Endpoint*)SReader,isBuiltin))
	{
		delete(SReader);
		return false;
	}
	m_allReaderList.push_back(SReader);

	*ReaderOut = SReader;
	return true;
}

void ParticipantImpl::registerReader(RTPSReader* SReader)
{
	eClock::my_sleep(30);
	m_userReaderList.push_back(SReader);
	m_builtinProtocols.addLocalReader(SReader);
}

void ParticipantImpl::registerWriter(RTPSWriter* SWriter)
{
	eClock::my_sleep(30);
	m_userWriterList.push_back(SWriter);
	m_builtinProtocols.addLocalWriter(SWriter);
}


bool ParticipantImpl::assignEndpointListenResources(Endpoint* endp,bool isBuiltin)
{
	bool valid = true;
	bool unicastempty = endp->unicastLocatorList.empty();
	bool multicastempty = endp->multicastLocatorList.empty();
	if(unicastempty && !isBuiltin && multicastempty)
	{
		std::string auxstr = endp->getEndpointKind() == WRITER ? "WRITER" : "READER";
		pWarning(auxstr << " created with no unicastLocatorList, adding default List"<<endl);
		for(LocatorListIterator lit = m_defaultUnicastLocatorList.begin();lit!=m_defaultUnicastLocatorList.end();++lit)
		{
			assignLocator2ListenResources(endp,lit,false,false);
		}
		endp->unicastLocatorList = m_defaultUnicastLocatorList;
	}
	else
	{
		for(LocatorListIterator lit = endp->unicastLocatorList.begin();lit!=endp->unicastLocatorList.end();++lit)
		{
			valid &= assignLocator2ListenResources(endp,lit,false,!isBuiltin);
		}
	}
	//MULTICAST
	if(multicastempty && !isBuiltin && unicastempty)
	{
		for(LocatorListIterator lit =m_defaultMulticastLocatorList.begin();lit!=m_defaultMulticastLocatorList.end();++lit)
		{
			valid &= assignLocator2ListenResources(endp,lit,true,false);
		}
		endp->multicastLocatorList = m_defaultMulticastLocatorList;
	}
	else
	{
		for(LocatorListIterator lit = endp->multicastLocatorList.begin();lit!=endp->multicastLocatorList.end();++lit)
		{
			valid &= assignLocator2ListenResources((Endpoint*)endp,lit,true,!isBuiltin);
		}
	}
	return valid;
}


bool ParticipantImpl::assignLocator2ListenResources(Endpoint* endp,LocatorListIterator lit,bool isMulti,bool isFixed)
{
	for(std::vector<ListenResource*>::iterator it = m_listenResourceList.begin();it!=m_listenResourceList.end();++it)
	{
		if((*it)->isListeningTo(*lit))
		{
			(*it)->addAssociatedEndpoint(endp);
			return true;
		}
	}
	ListenResource* LR = new ListenResource(this);
	Locator_t loc = LR->init_thread(*lit,isMulti,isFixed);
	if(loc.kind>0)
	{
		LR->addAssociatedEndpoint(endp);
		*lit = loc;
		m_listenResourceList.push_back(LR);
		return true;
	}
	else
	{
		delete(LR);
		return false;
	}
}




bool ParticipantImpl::deleteUserEndpoint(Endpoint* p_endpoint,char type)
{
	bool found = false;
	{
	boost::lock_guard<Endpoint> guard(*p_endpoint);
	if(type == 'W')
	{
		for(p_WriterIterator wit=m_userWriterList.begin();
				wit!=m_userWriterList.end();++wit)
		{
			if((*wit)->getGuid().entityId == p_endpoint->getGuid().entityId) //Found it
			{
				m_userWriterList.erase(wit);
				found = true;
				break;
			}
		}
	}
	if(type == 'R')
	{
		for(p_ReaderIterator rit=m_userReaderList.begin()
				;rit!=m_userReaderList.end();++rit)
		{
			if((*rit)->getGuid().entityId == p_endpoint->getGuid().entityId) //Found it
			{
				m_userReaderList.erase(rit);
				found = true;
				break;
			}
		}
	}
	if(!found)
		return false;
	if(p_endpoint->getEndpointKind()==WRITER)
		m_builtinProtocols.removeLocalWriter((RTPSWriter*)p_endpoint);
	else
		m_builtinProtocols.removeLocalReader((RTPSReader*)p_endpoint);
	//Remove it from threadListenList
	std::vector<ListenResource*>::iterator thit;
	for(thit=m_listenResourceList.begin();
			thit!=m_listenResourceList.end();thit++)
	{
		(*thit)->removeAssociatedEndpoint(p_endpoint);
    }
	for(thit=m_listenResourceList.begin();
			thit!=m_listenResourceList.end();thit++)
	{
		if(!(*thit)->hasAssociatedEndpoints())
		{
						delete(*thit);
			m_listenResourceList.erase(thit);
			break;
		}
	}

	}
	delete(p_endpoint);
	return true;
}

void ParticipantImpl::announceParticipantState()
{
	this->m_builtinProtocols.announceParticipantState();
}

void ParticipantImpl::stopParticipantAnnouncement()
{
	this->m_builtinProtocols.stopParticipantAnnouncement();
}

void ParticipantImpl::resetParticipantAnnouncement()
{
	this->m_builtinProtocols.resetParticipantAnnouncement();
}

void ParticipantImpl::ResourceSemaphorePost()
{
	if(mp_ResourceSemaphore!=NULL)
	{
		mp_ResourceSemaphore->post();
	}
}

void ParticipantImpl::ResourceSemaphoreWait()
{
	if(mp_ResourceSemaphore!=NULL)
	{
		mp_ResourceSemaphore->wait();
	}
	
}

bool ParticipantImpl::newRemoteEndpointDiscovered(const GUID_t& pguid, int16_t userDefinedId,EndpointKind_t kind)
{
	if(m_builtin.use_STATIC_EndpointDiscoveryProtocol == false)
	{
		pWarning("Remote Endpoints can only be activated with static discovery protocol");
		return false;
	}
	return m_builtinProtocols.mp_PDP->newRemoteEndpointStaticallyDiscovered(pguid,userDefinedId,kind);
}

} /* namespace rtps */
} /* namespace eprosima */

