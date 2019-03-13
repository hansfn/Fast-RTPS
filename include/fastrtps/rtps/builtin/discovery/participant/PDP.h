// Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file PDP.h
 *
 */

#ifndef PDP_H_
#define PDP_H_
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <mutex>
#include "../../../common/Guid.h"
#include "../../../attributes/RTPSParticipantAttributes.h"
#include "../../../../qos/QosPolicies.h"



namespace eprosima {
namespace fastrtps{
namespace rtps {

class RTPSWriter;
class RTPSReader;
class WriterHistory;
class ReaderHistory;
class RTPSParticipantImpl;
class BuiltinProtocols;
class EDP;
class RemoteParticipantLeaseDuration;
class ReaderProxyData;
class WriterProxyData;
class ParticipantProxyData;
class PDPListener;


/**
 * Abstract class PDP that implements the basic interfaces for all Participant Discovery implementations
 * It also keeps the Participant Discovery Data and provides interfaces to access it
 *@ingroup DISCOVERY_MODULE
 */
class PDP
{
    friend class RemoteRTPSParticipantLeaseDuration;
    friend class PDPListener;
    public:
    /**
     * Constructor
     * @param builtin Pointer to the BuiltinProcols object.
     */
    PDP(BuiltinProtocols* builtin);
    virtual ~PDP();

    virtual void initializeParticipantProxyData(ParticipantProxyData* participant_data);

    /**
     * Initialize the PDP.
     * @param part Pointer to the RTPSParticipant.
     * @return True on success
     */
    virtual bool initPDP(RTPSParticipantImpl* part);

    /**
     * Force the sending of our local DPD to all remote RTPSParticipants and multicast Locators.
     * @param new_change If true a new change (with new seqNum) is created and sent; if false the last change is re-sent
     * @param dispose Sets change kind to NOT_ALIVE_DISPOSED_UNREGISTERED 
     */
    virtual void announceParticipantState(bool new_change, bool dispose = false);

    //!Stop the RTPSParticipantAnnouncement (only used in tests).
    virtual void stopParticipantAnnouncement() = 0;

    //!Reset the RTPSParticipantAnnouncement (only used in tests).
    virtual void resetParticipantAnnouncement() = 0;

    /**
     * Add a ReaderProxyData to the correct ParticipantProxyData.
     * @param rdata Pointer to the ReaderProxyData objectr to add.
     * @param pdata
     * @return True if correct.
     */
    bool addReaderProxyData(ReaderProxyData* rdata, ParticipantProxyData &pdata_out);

    /**
     * Add a WriterProxyData to the correct ParticipantProxyData.
     * @param wdata Pointer to the WriterProxyData objectr to add.
     * @param pdata
     * @return True if correct.
     */
    bool addWriterProxyData(WriterProxyData* wdata, ParticipantProxyData &pdata);

    /**
     * This method returns a pointer to a ReaderProxyData object if it is found among the registered RTPSParticipants (including the local RTPSParticipant).
     * @param[in] reader GUID_t of the reader we are looking for.
     * @param rdata Pointer to pointer of the ReaderProxyData object.
     * @param pdata Pointer to pointer of the ParticipantProxyData object.
     * @return True if found.
     */
    bool lookupReaderProxyData(const GUID_t& reader, ReaderProxyData& rdata, ParticipantProxyData& pdata);
    /**
     * This method returns a pointer to a WriterProxyData object if it is found among the registered RTPSParticipants (including the local RTPSParticipant).
     * @param[in] writer GUID_t of the writer we are looking for.
     * @param wdata Pointer to pointer of the WriterProxyData object.
     * @param pdata Pointer to pointer of the ParticipantProxyData object.
     * @return True if found.
     */
    bool lookupWriterProxyData(const GUID_t& writer, WriterProxyData& wdata, ParticipantProxyData& pdata);
    /**
     * This method returns a pointer to a RTPSParticipantProxyData object if it is found among the registered RTPSParticipants.
     * @param[in] pguid GUID_t of the RTPSParticipant we are looking for.
     * @param pdata Copy information on ParticipantProxyData object.
     * @return True if found.
     */
    bool lookupParticipantProxyData(const GUID_t& pguid, ParticipantProxyData& pdata);
    /**
     * This method removes and deletes a ReaderProxyData object from its corresponding RTPSParticipant.
     * @return true if found and deleted.
     */
    bool removeReaderProxyData(const GUID_t& reader_guid);
    /**
     * This method removes and deletes a WriterProxyData object from its corresponding RTPSParticipant.
     * @return true if found and deleted.
     */
    bool removeWriterProxyData(const GUID_t& writer_guid);

    /**
     * Create the SPDP Writer and Reader
     * @return True if correct.
     */
    virtual bool createPDPEndpoints() = 0;

    /**
     * This method assigns remote endpoints to the builtin endpoints defined in this protocol. It also calls the corresponding methods in EDP and WLP.
     * @param pdata Pointer to the RTPSParticipantProxyData object.
     */
    virtual void assignRemoteEndpoints(ParticipantProxyData* pdata) = 0;

    void notifyAboveRemoteEndpoints(const ParticipantProxyData& pdata);

    /**
     * Remove remote endpoints from the participant discovery protocol
     * @param pdata Pointer to the ParticipantProxyData to remove
     */
    virtual void removeRemoteEndpoints(ParticipantProxyData* pdata) = 0;

    /**
     * This method removes a remote RTPSParticipant and all its writers and readers.
     * @param partGUID GUID_t of the remote RTPSParticipant.
     * @return true if correct.
     */
    bool removeRemoteParticipant(GUID_t& partGUID);

    //!Pointer to the builtin protocols object.
    BuiltinProtocols* mp_builtin;
    /**
     * Get a pointer to the local RTPSParticipant RTPSParticipantProxyData object.
     * @return Pointer to the local RTPSParticipant RTPSParticipantProxyData object.
     */
    inline ParticipantProxyData* getLocalParticipantProxyData()
    {
        return m_participantProxies.front();
    }
    /**
     * Get a pointer to the EDP object.
     * @return pointer to the EDP object.
     */
    inline EDP* getEDP(){return mp_EDP;}
    
    /**
     * Get a cons_iterator to the beginning of the RTPSParticipant Proxies.
     * @return const_iterator.
     */
    inline  std::vector<ParticipantProxyData*>::const_iterator ParticipantProxiesBegin(){return m_participantProxies.begin();};
    
    /**
     * Get a cons_iterator to the end RTPSParticipant Proxies.
     * @return const_iterator.
     */
    inline std::vector<ParticipantProxyData*>::const_iterator ParticipantProxiesEnd(){return m_participantProxies.end();};

    /**
     * Assert the liveliness of a Remote Participant.
     * @param guidP GuidPrefix_t of the participant whose liveliness is being asserted.
     */
    void assertRemoteParticipantLiveliness(const GuidPrefix_t& guidP);

    /**
     * Assert the liveliness of a Local Writer.
     * @param kind LivilinessQosPolicyKind to be asserted.
     */
    void assertLocalWritersLiveliness(LivelinessQosPolicyKind kind);

    /**
     * Assert the liveliness of remote writers.
     * @param guidP GuidPrefix_t of the participant whose writers liveliness is begin asserted.
     * @param kind LivelinessQosPolicyKind of the writers.
     */
    void assertRemoteWritersLiveliness(GuidPrefix_t& guidP,LivelinessQosPolicyKind kind);

    /**
     * Get the RTPS participant
     * @return RTPS participant
     */
    inline RTPSParticipantImpl* getRTPSParticipant() const {return mp_RTPSParticipant;};
    /**
     * Get the mutex.
     * @return Pointer to the Mutex
     */
    inline std::recursive_mutex* getMutex() const {return mp_mutex;}

    CDRMessage_t get_participant_proxy_data_serialized(Endianness_t endian);

    protected:
    //!Pointer to the local RTPSParticipant.
    RTPSParticipantImpl* mp_RTPSParticipant;
    //!Discovery attributes.
    BuiltinAttributes m_discovery;
    //!Pointer to the PDPWriter.
    RTPSWriter* mp_PDPWriter;
    //!Pointer to the PDPReader.
    RTPSReader* mp_PDPReader;
    //!Pointer to the EDP object.
    EDP* mp_EDP;
    //!Registered RTPSParticipants (including the local one, that is the first one.)
    std::vector<ParticipantProxyData*> m_participantProxies;
    //!Variable to indicate if any parameter has changed.
    bool m_hasChangedLocalPDP;
    //!Listener for the SPDP messages.
    PDPListener* mp_listener;
    //!WriterHistory
    WriterHistory* mp_PDPWriterHistory;
    //!Reader History
    ReaderHistory* mp_PDPReaderHistory;
    //!Participant data atomic access assurance
    std::recursive_mutex* mp_mutex;

};

}
} /* namespace rtps */
} /* namespace eprosima */
#endif
#endif /* PDP_H_ */