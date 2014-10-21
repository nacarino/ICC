/*
 * ndn-priconsumer.cc
 *
 *  Created on: Oct 9, 2014
 *      Author: jelfn
 */

#include "ndn-priconsumer.h"

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED (PriConsumer);

TypeId
PriConsumer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::PriConsumer")
    .SetGroupName ("Ndn")
    .SetParent<ConsumerCbr> ()
    .AddConstructor<PriConsumer> ()
    ;

  return tid;
}

std::set<uint32_t>
PriConsumer::GetSeqTimeout ()
{
	std::cout << "Entering PriConsumer GetSeqTimeout" << std::endl;
	std::set<uint32_t> currSeqs;
	uint32_t seqNo;



	std::cout << "Test the inheritance: " << m_seq << std::endl;
	std::cout << "Does any of this make sense?" << std::endl;
	uint32_t curr = m_seqTimeouts.size();

	std::cout << "I have a timeout size of: " << curr << std::endl;

	SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry = m_seqTimeouts.get<i_timestamp> ().begin ();

	for (; entry != m_seqTimeouts.get<i_timestamp> ().end (); entry++ )
	{
		std::cout << "Attempting a grab" << std::endl;

		seqNo = entry->seq;

		std::cout << "I have obtained a seq of: " << seqNo << " inserting..." << std::endl;

		currSeqs.insert(seqNo);


	}

	std::cout << "Returning from GetSeqTimeout" << std::endl;

	return currSeqs;
}

} /* namespace ndn */
} /* namespace ns3 */
