/* -*- Mode:C++; c-file-style:"gnu"; -*- */
/*
 *
 * Copyright (c) 2014 Waseda University, Sato Laboratory
 * Author: Jairo Eduardo Lopez <jairo@ruri.waseda.jp>
 *
 * Special thanks to University of Washington for initial templates
 *
 *  smart-flooding-inf.h is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  smart-flooding-inf.h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero Public License for more details.
 *
 *  You should have received a copy of the GNU Affero Public License
 *  along with smart-flooding-inf.h.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SMART_FLOODING_INF_H_
#define SMART_FLOODING_INF_H_

#include <ns3-dev/ns3/ndnSIM/model/cs/ndn-content-store.h>
#include <ns3-dev/ns3/ndnSIM/model/fw/smart-flooding.h>
#include <ns3-dev/ns3/ndnSIM/model/ndn-data.h>
#include <ns3-dev/ns3/ndnSIM/model/ndn-interest.h>
#include <ns3-dev/ns3/ndnSIM/model/ndn-name.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry.h>

#include <ns3-dev/ns3/assert.h>
#include <ns3-dev/ns3/boolean.h>
#include <ns3-dev/ns3/log.h>
#include <ns3-dev/ns3/random-variable.h>
#include <ns3-dev/ns3/simulator.h>

#include <boost/ref.hpp>
#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
namespace ll = boost::lambda;

namespace ns3 {
namespace ndn {
namespace fw {

class SmartFloodingInf : public SmartFlooding {
public:
	static TypeId
	GetTypeId ();

	SmartFloodingInf();
	virtual ~SmartFloodingInf();

	virtual void
	SatisfyPendingInterest (Ptr<Face> inFace, // 0 allowed (from cache)
			 Ptr<const Data> data,
			 Ptr<pit::Entry> pitEntry);

	virtual void
	OnData (Ptr<Face> face,
			Ptr<Data> data);

	Time m_start;
	Time m_stop;
	bool m_redirect;
	bool m_data_redirect;

	std::set<Ptr<Face> > redirectFaces;
	std::set<Ptr<Face> > dataRedirect;

};

} /* namespace fw */
} /* namespace ndn */
} /* namespace ns3 */

#endif /* SMART_FLOODING_INF_H_ */
