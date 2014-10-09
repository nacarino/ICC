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
	std::set<uint32_t> currSeqs;
	uint32_t seqNo;

	std::cout << "About to enter for loop" << std::endl;
	if (m_seqTimeouts.size () != 0)
	{
		std::cout << "Test iterator" << std::endl;
		SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
				m_seqTimeouts.get<i_timestamp> ().begin () ;

		seqNo = entry->seq;
		std::cout << "Obtained one: " << seqNo << std::endl;
		currSeqs.insert(seqNo);
	}

	std::cout << "Returning from GetSeqTimeout" << std::endl;

	return currSeqs;
}

} /* namespace ndn */
} /* namespace ns3 */
