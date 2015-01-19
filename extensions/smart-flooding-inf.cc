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

NS_LOG_COMPONENT_DEFINE ("SmartFloodingInf");

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
, m_rtx           (MilliSeconds(100))
, m_redirect      (false)
, m_data_redirect (false)
, m_edge          (false)
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
  if (m_redirect) {

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

  // Iterator
  std::set<Ptr<Face> >::iterator it;

  // Lookup PIT entry
  Ptr<pit::Entry> pitEntry = m_pit->Lookup (*data);
  if (pitEntry == 0)
    {

      if (m_data_redirect)
	{
	  // Add to content store
	  m_contentStore->Add (data);

	  // Save the information in our map and wait for further instructions
	  if (m_edge)
	    {
	      if (m_passthrough)
		{
		  for (it = dataRedirect.begin(); it != dataRedirect.end(); it++)
		    {
		      Ptr<Face> touse = (*it);
		      // The Data we have received, doesn't have a PIT entry, so we create it
		      // Create a Nonce
		      UniformVariable m_rand = UniformVariable(0, std::numeric_limits<uint32_t>::max ());

		      // Obtain the name from the Data packet
		      Ptr<ndn::Name> incoming_name = Create<ndn::Name> (data->GetName());

		      // Create the Interest packet using the information we have
		      Ptr<Interest> interest = Create<Interest> ();
		      interest->SetNonce               (m_rand.GetValue ());
		      interest->SetName                (incoming_name);
		      interest->SetInterestLifetime    (Years (1));

		      // Create a newly created PIT Entry
		      pitEntry = m_pit->Create (interest);
		      if (pitEntry != 0)
			{
			  DidCreatePitEntry (touse, interest, pitEntry);
			}

		      pitEntry->AddSeenNonce (interest->GetNonce ());

		      pitEntry->AddIncoming(touse);

		      pitEntry->UpdateLifetime(interest->GetInterestLifetime ());

		      // Do data plane performance measurements
		      WillSatisfyPendingInterest (inFace, pitEntry);

		      // Actually satisfy pending interest
		      SatisfyPendingInterest (inFace, data, pitEntry);
		    }
		}
	      else
		{
		  for (it = dataRedirect.begin(); it != dataRedirect.end(); it++)
		    {
		      superData curr;
		      curr.inface = inFace;
		      curr.outface = (*it);
		      curr.data = data;
		      buffer[now] = curr;
		    }
		}
	      return;
	    }
	  else
	    {
	      for (it = dataRedirect.begin(); it != dataRedirect.end(); it++)
		{
		  Ptr<Face> touse = (*it);
		  // The Data we have received, doesn't have a PIT entry, so we create it
		  // Create a Nonce
		  UniformVariable m_rand = UniformVariable(0, std::numeric_limits<uint32_t>::max ());

		  // Obtain the name from the Data packet
		  Ptr<ndn::Name> incoming_name = Create<ndn::Name> (data->GetName());

		  // Create the Interest packet using the information we have
		  Ptr<Interest> interest = Create<Interest> ();
		  interest->SetNonce               (m_rand.GetValue ());
		  interest->SetName                (incoming_name);
		  interest->SetInterestLifetime    (Years (1));

		  // Create a newly created PIT Entry
		  pitEntry = m_pit->Create (interest);
		  if (pitEntry != 0)
		    {
		      DidCreatePitEntry (touse, interest, pitEntry);
		    }

		  pitEntry->AddSeenNonce (interest->GetNonce ());

		  pitEntry->AddIncoming(touse);

		  pitEntry->UpdateLifetime(interest->GetInterestLifetime ());

		  // Do data plane performance measurements
		  WillSatisfyPendingInterest (inFace, pitEntry);

		  // Actually satisfy pending interest
		  SatisfyPendingInterest (inFace, data, pitEntry);
		}

	      return;
	    }
	}
      else
	{
	  DidReceiveUnsolicitedData (inFace, data, false);
	  return;
	}
    }
  else
    {
      bool cached = m_contentStore->Add(data);
      DidReceiveSolicitedData (inFace, data, cached);
    }

  superData curr;
  curr.data = data;
  curr.outface = 0;
  curr.inface = inFace;
  buffer[now] = curr;

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

void
SmartFloodingInf::WillEraseTimedOutPendingInterest (Ptr<pit::Entry> pitEntry)
{
  NS_LOG_DEBUG ("WillEraseTimedOutPendingInterest for " << pitEntry->GetPrefix ());

  if (pitEntry != 0) {
      for (pit::Entry::out_container::iterator face = pitEntry->GetOutgoing ().begin ();
	  face != pitEntry->GetOutgoing ().end ();
	  face ++)
	{
	  // NS_LOG_DEBUG ("Face: " << face->m_face);
	  pitEntry->GetFibEntry ()->UpdateStatus (face->m_face, fib::FaceMetric::NDN_FIB_YELLOW);
	}

      super::WillEraseTimedOutPendingInterest (pitEntry);
  }
}

void
SmartFloodingInf::WillSatisfyPendingInterest (Ptr<Face> inFace, Ptr<pit::Entry> pitEntry)
{
  if (inFace != 0)
    {
      // Update metric status for the incoming interface in the corresponding FIB entry
      pitEntry->GetFibEntry ()->UpdateStatus (inFace, fib::FaceMetric::NDN_FIB_GREEN);
    }

  if (pitEntry != 0)
    {
      pit::Entry::out_iterator out = pitEntry->GetOutgoing ().find (inFace);

      // If we have sent interest for this data via this face, then update stats.
      if (out != pitEntry->GetOutgoing ().end ())
	{
	  pitEntry->GetFibEntry ()->UpdateFaceRtt (inFace, Simulator::Now () - out->m_sendTime);
	}
    }

  m_satisfiedInterests (pitEntry);
}

void
SmartFloodingInf::flushBuffer (Ptr<Face> face) {

  // Get our current time
  Time now = Simulator::Now ();

  std::map<Time, superData>::iterator ij;

  std::string lastname;

  int total = 0;

  std::cout << "Historical buffer of " << buffer.size () << " at " << now << std::endl;
  std::cout << "Flushing from " << m_start +m_rtx << std::endl;

  for (ij = buffer.lower_bound(m_start + m_rtx) ; ij != buffer.end(); ++ij)
    {

      Ptr<pit::Entry> pitEntry = m_pit->Lookup (*(*ij).second.data);
      if (pitEntry != 0)
	{
	  std::cout << "I have a PIT entry for this, why?" << std::endl;
	}
      else
	{
	  std::cout << "No PIT, must create" << std::endl;
	  // Create a Nonce
	  UniformVariable m_rand = UniformVariable(0, std::numeric_limits<uint32_t>::max ());

	  // Obtain the name from the Data packet
	  Ptr<ndn::Name> incoming_name = Create<ndn::Name> ((*ij).second.data->GetName());

//	  // Create the Interest packet using the information we have
//	  Ptr<Interest> interest = Create<Interest> ();
//	  interest->SetNonce               (m_rand.GetValue ());
//	  interest->SetName                (incoming_name);
//	  interest->SetInterestLifetime    (Years (1));
//
//	  Create a newly created PIT Entry
//	  Ptr<pit::Entry> pitEntry = m_pit->Create (interest);
//	  if (pitEntry != 0)
//	    {
//	      DidCreatePitEntry ((*ij).second.inface, interest, pitEntry);
//	    }
	  //
	  //      pitEntry->AddSeenNonce (interest->GetNonce ());
	  lastname = incoming_name->toUri();

//	  Ptr<Face> touse = (*ij).second.outface;
//	  if (touse == 0)
//	    {
//	      // Iterator
//	      std::set<Ptr<Face> >::iterator it;
//	      for (it = dataRedirect.begin(); it != dataRedirect.end(); it++)
//		{
//		  //pitEntry->AddIncoming ((*it));
//		  (*it)->SendData((*ij).second.data);
//		  std::cout << "Sent out " << lastname << " through face " << (*it) << std::endl;
//		}
//	    }
//	  else
//	    {
	      //pitEntry->AddIncoming (touse);
	      face->SendData((*ij).second.data);
	      std::cout << "Information present Sent out " << lastname << " through face " << face->GetId() << std::endl;
//	    }

//	  pitEntry->UpdateLifetime(interest->GetInterestLifetime ());
//
//	  WillSatisfyPendingInterest ((*ij).second.inface, pitEntry);
//	  SatisfyPendingInterest((*ij).second.inface, (*ij).second.data, pitEntry);

	  total++;
	}
    }

  std::cout << "Transmitted " << total << std::endl;
  std::cout << "Last name transmitted " << lastname << std::endl;
}

uint32_t
SmartFloodingInf::bufferSize () {
  return buffer.size ();
}

} /* namespace fw */
} /* namespace ndn */
} /* namespace ns3 */
