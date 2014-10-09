/*
 * ndn-priconsumer.h
 *
 *  Created on: Oct 9, 2014
 *      Author: jelfn
 */

#ifndef NDN_PRICONSUMER_H_
#define NDN_PRICONSUMER_H_

#include <set>

#include <ns3-dev/ns3/ptr.h>
#include <ns3-dev/ns3/log.h>
#include <ns3-dev/ns3/simulator.h>
#include <ns3-dev/ns3/packet.h>
#include <ns3-dev/ns3/callback.h>
#include <ns3-dev/ns3/string.h>
#include <ns3-dev/ns3/boolean.h>
#include <ns3-dev/ns3/uinteger.h>
#include <ns3-dev/ns3/integer.h>
#include <ns3-dev/ns3/double.h>
#include <ns3-dev/ns3/type-id.h>
#include <ns3-dev/ns3/ndnSIM/model/ndn-name.h>
#include <ns3-dev/ns3/ndnSIM/apps/ndn-consumer-cbr.h>

namespace ns3 {
namespace ndn {

class PriConsumer: public ConsumerCbr {
public:
	static TypeId GetTypeId ();

	std::set<uint32_t>
	GetSeqTimeout ();

};

} /* namespace ndn */
} /* namespace ns3 */

#endif /* NDN_PRICONSUMER_H_ */
