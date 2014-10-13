/* -*- Mode:C++; c-file-style:"gnu"; -*- */
/*
 *
 * Copyright (c) 2014 Waseda University, Sato Laboratory
 * Author: Jairo Eduardo Lopez <jairo@ruri.waseda.jp>
 *
 * Special thanks to University of Washington for initial templates
 *
 *  smart-flooding-inf.cc is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  smart-flooding-inf.cc is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero Public License for more details.
 *
 *  You should have received a copy of the GNU Affero Public License
 *  along with smart-flooding-inf.cc.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "smart-flooding-inf.h"

namespace ns3 {
namespace ndn {
namespace fw {

NS_OBJECT_ENSURE_REGISTERED (SmartFloodingInf);

TypeId
SmartFloodingInf::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::ndn::fw::SmartFloodingInf")
		   .SetGroupName ("Ndn")
		   .SetParent <SmartFlooding> ()
		   .AddConstructor <SmartFloodingInf> ()
		   ;
	return tid;
}

SmartFloodingInf::SmartFloodingInf()
 : m_start         (0)
 , m_stop          (0)
 , m_redirect      (false)
 , m_data_redirect (false)
{
}

SmartFloodingInf::~SmartFloodingInf() {
	// TODO Auto-generated destructor stub
}

// Reimplementation to create the Interests and redirect them
void
SmartFloodingInf::SatisfyPendingInterest (Ptr<Face> inFace, Ptr<const Data> data, Ptr<pit::Entry> pitEntry)
{
	if (inFace != 0)
		pitEntry->RemoveIncoming (inFace);

	std::set<Ptr<Face> > seen_face;

	//satisfy all pending incoming Interests
	BOOST_FOREACH (const pit::IncomingFace &incoming, pitEntry->GetIncoming ())
	{
		bool ok = incoming.m_face->SendData (data);

		DidSendOutData (inFace, incoming.m_face, data, pitEntry);
		NS_LOG_DEBUG ("Satisfy " << *incoming.m_face);

		if (!ok)
		{
			m_dropData (data, incoming.m_face);
			NS_LOG_DEBUG ("Cannot satisfy data to " << *incoming.m_face);
		}

		// Keep list of Faces seen to later check
		seen_face.insert(incoming.m_face);
	}
	Time now = Simulator::Now ();

	// Check if we have the redirect turned on
	if (m_redirect && m_start <= now && now <= m_stop) {
		// Iterator
		std::set<Ptr<Face> >::iterator it;

		// Place to keep the difference of the set we have and what we have seen
		std::set<Ptr<Face> > diff;

		std::set_difference(redirectFaces.begin(), redirectFaces.end(), seen_face.begin(), seen_face.end(),
				std::inserter(diff, diff.begin()));

		for (it = diff.begin(); it != diff.end(); it++)
		{
			Ptr<Face> touse = (*it);
			bool ok = touse->SendData (data);

			DidSendOutData (inFace, touse, data, pitEntry);
			NS_LOG_DEBUG ("Satisfy " << touse);

			if (!ok)
			{
				m_dropData (data, touse);
				NS_LOG_DEBUG ("Cannot satisfy data to " << touse);
			}
		}
	}

	// All incoming interests are satisfied. Remove them
	pitEntry->ClearIncoming ();

	// Remove all outgoing faces
	pitEntry->ClearOutgoing ();

	// Set pruning timeout on PIT entry (instead of deleting the record)
	m_pit->MarkErased (pitEntry);
}

// Reimplementation on obtaining Data
void
SmartFloodingInf::OnData (Ptr<Face> inFace, Ptr<Data> data)
{
	NS_LOG_FUNCTION (inFace << data->GetName ());
	m_inData (data, inFace);

	Time now = Simulator::Now ();

	// Check if we are in the redirection time
	if (m_data_redirect && m_start <= now && now <= m_stop)
	{
		// Iterator
		std::set<Ptr<Face> >::iterator it;

		Ptr<pit::Entry> pitEntry = m_pit->Lookup (*data);
		if (pitEntry == 0)
		{
			// If so, there is a huge chance that the Data we have received, doesn't
			// have a PIT entry, so we create it


			// Create a Nonce
			UniformVariable m_rand = UniformVariable(0, std::numeric_limits<uint32_t>::max ());

			// Obtain the name from the Data packet
			Ptr<ndn::Name> incoming_name = Create<ndn::Name> (data->GetName());

			// Create the Interest packet using the information we have
			Ptr<Interest> interest = Create<Interest> ();
			interest->SetNonce               (m_rand.GetValue ());
			interest->SetName                (incoming_name);
			interest->SetInterestLifetime    (Seconds (2));

			// Create a newly created PIT Entry
			pitEntry = m_pit->Create (interest);

			// Add all the interfaces specified by dataRedirect
			for (it = dataRedirect.begin(); it != dataRedirect.end(); it++)
			{
				pitEntry->AddIncoming(*it);
			}
		} else
		{
			for (it = dataRedirect.begin(); it != dataRedirect.end(); it++)
			{
				pitEntry->AddIncoming(*it);
			}
		}
	}

	// Lookup PIT entry
	Ptr<pit::Entry> pitEntry = m_pit->Lookup (*data);
	if (pitEntry == 0)
	{
		bool cached = false;

		if (m_cacheUnsolicitedData || (m_cacheUnsolicitedDataFromApps && (inFace->GetFlags () & Face::APPLICATION)))
		{
			// Optimistically add or update entry in the content store
			cached = m_contentStore->Add (data);
		}
		else
		{
			// Drop data packet if PIT entry is not found
			// (unsolicited data packets should not "poison" content store)

			//drop dulicated or not requested data packet
			m_dropData (data, inFace);
		}

		DidReceiveUnsolicitedData (inFace, data, cached);
		return;
	}
	else
	{
		bool cached = m_contentStore->Add (data);
		DidReceiveSolicitedData (inFace, data, cached);
	}

	while (pitEntry != 0)
	{
		// Do data plane performance measurements
		WillSatisfyPendingInterest (inFace, pitEntry);

		// Actually satisfy pending interest
		SatisfyPendingInterest (inFace, data, pitEntry);

		// Lookup another PIT entry
		pitEntry = m_pit->Lookup (*data);
	}
}

} /* namespace fw */
} /* namespace ndn */
} /* namespace ns3 */
