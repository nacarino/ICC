/* -*- Mode:C++; c-file-style:"gnu"; -*- */
/*
 * icc-scenario.cc
 *  Sector walk for ndnSIM
 *
 * Copyright (c) 2014 Waseda University, Sato Laboratory
 * Author: Jairo Eduardo Lopez <jairo@ruri.waseda.jp>
 *         Takahiro Miyamoto <mt3.mos@gmail.com>
 *         Zhu Li <philipszhuli@ruri.waseda.jp>
 *
 * Special thanks to University of Washington for initial templates
 *
 *  icc-scenario.cc is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  icc-scenario.cc is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero Public License for more details.
 *
 *  You should have received a copy of the GNU Affero Public License
 *  along with icc-scenario.cc.  If not, see <http://www.gnu.org/licenses/>.
 */

// Standard C++ modules
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <iostream>
#include <map>
#include <string>
#include <sys/time.h>
#include <vector>

// Random modules
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/variate_generator.hpp>

// ns3 modules
#include <ns3-dev/ns3/applications-module.h>
#include <ns3-dev/ns3/bridge-helper.h>
#include <ns3-dev/ns3/csma-module.h>
#include <ns3-dev/ns3/core-module.h>
#include <ns3-dev/ns3/mobility-module.h>
#include <ns3-dev/ns3/network-module.h>
#include <ns3-dev/ns3/point-to-point-module.h>
#include <ns3-dev/ns3/wifi-module.h>

// ndnSIM modules
#include <ns3-dev/ns3/ndnSIM-module.h>
#include <ns3-dev/ns3/ndnSIM/utils/tracers/ipv4-rate-l3-tracer.h>
#include <ns3-dev/ns3/ndnSIM/utils/tracers/ipv4-seqs-app-tracer.h>

using namespace ns3;
using namespace boost;
using namespace std;
using namespace ndn;
using namespace fib;

namespace br = boost::random;

// Pit Entries
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-impl.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry-impl.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry-incoming-face.h>
#include <ns3-dev/ns3/ndnSIM/model/pit/ndn-pit-entry-outgoing-face.h>

#include <ns3-dev/ns3/ndnSIM/apps/ndn-consumer.h>
#include <ns3-dev/ns3/ndnSIM/apps/ndn-consumer-cbr.h>

#include "ndn-priconsumer.h"
#include "smart-flooding-inf.h"


typedef struct timeval TIMER_TYPE;
#define TIMER_NOW(_t) gettimeofday (&_t,NULL);
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec*1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1)-TIMER_SECONDS (_t2))

char scenario[250] = "ICCScenario";

NS_LOG_COMPONENT_DEFINE (scenario);

// Number generator
br::mt19937_64 gen;

// Global information to use in callbacks
std::set<uint32_t> res;
std::map<Mac48Address,Ptr<Node> > seen_macs;
std::map<int, Ptr<Node> > numToNode;
std::map<std::string, Ptr<Node> > ssidToNode;
std::map<std::string, int> ssidToNum;
NodeContainer NCaps;
NodeContainer NCcenters;
NodeContainer NCmobiles;
NodeContainer NCservers;

std::vector<Mac48Address> mac_queue;
uint32_t numqueue = 0;
bool sectorChange = false;
bool readEntry = false;
std::string ssidOld = "";

std::vector<YansWifiPhyHelper> yanhelpers;
std::map<uint32_t, Ptr<YansWifiChannel> > channels;

// Obtains a random number from a uniform distribution between min and max.
// Must seed number generator to ensure randomness at runtime.
int obtain_Num(int min, int max)
{
  br::uniform_int_distribution<> dist(min, max);
  return dist(gen);
}

// Obtain a random double from a uniform distribution between min and max.
// Must seed number generator to ensure randomness at runtime.
double obtain_Num(double min, double max)
{
  br::uniform_real_distribution<> dist(min, max);
  return dist(gen);
}



void PrintSeqs (Ptr<PriConsumer> consumer)
{
  cout << "Printing sequence numbers to distribute " << Simulator::Now () << endl;

  std::set<uint32_t> seqset = consumer->GetSeqTimeout ();
  cout << "Survived the class grab" << endl;
  std::set<uint32_t>::iterator it;

  std::set<uint32_t> diff;

  std::set_difference(seqset.begin(), seqset.end(), res.begin(), res.end(),
		      std::inserter(diff, diff.begin()));

  cout << "Retransmission packet number: " << diff.size () << endl;

  std::cout << "diff now contains:";
  for (it=diff.begin(); it!=diff.end(); ++it)
    std::cout << ' ' << *it;
  std::cout << '\n';

  res.insert(diff.begin(), diff.end());

  cout << "res size is " << res.size() << endl;

  std::cout << "res now contains:";
  for (it=res.begin(); it!=res.end(); ++it)
    std::cout << ' ' << *it;
  std::cout << '\n';

  cout << "______________________________" << endl;
}

// Function to change the SSID of a node's Wifi netdevice and obtain the packets to redirect
void
SetSSID (uint32_t mtId, uint32_t deviceId, Ssid ssidName)
{
  cout << "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" << endl;
  Time now = Simulator::Now ();

  cout << "Set SSID " << ssidName << " to device " << deviceId << " at " <<  now << endl;
  char configbuf[250];

  sprintf(configbuf, "/NodeList/%d/DeviceList/%d/$ns3::WifiNetDevice/Mac/Ssid", mtId, deviceId);

  Config::Set(configbuf, SsidValue(ssidName));

  //	if (deviceId > 0)
  //	{
  //		cout << "Set SSID " << ssidName << " to device " << deviceId-1 << " at " <<  now << endl;
  //		sprintf(configbuf, "/NodeList/%d/DeviceList/%d/$ns3::WifiNetDevice/Mac/Ssid", mtId, deviceId-1);
  //
  //		Config::Set(configbuf, SsidValue(ssidName));
  //	}

  //	sprintf(configbuf, "/NodeList/%d/DeviceList/%d/$ns3::WifiNetDevice/Channel", mtId, deviceId);
  //
  //	Config::Set(configbuf, PointerValue(channels[mtId]));

  sectorChange = true;
  cout << "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" << endl;
}

/**
 * \brief NDN to act as if a NNN INF packet was received
 * \param n_node The node you wish to manipulate
 * \param faceId The node relative Face Id you wish to add to the PITs
 */
void
INFObtained (Ptr<Node> n_node, uint32_t faceId)
{
  cout << "Entering INFObtained" << endl;

  Ptr<L3Protocol> protocol = n_node->GetObject <L3Protocol> ();
  Ptr<Pit> pit = n_node->GetObject <Pit> ();
  Ptr<Face> n_face = protocol->GetFace(faceId);

  // Cycle through the PIT entries for the node
  for ( Ptr<pit::Entry> entry = pit->Begin(); entry != pit->End(); entry = pit->Next(entry))
    {
      bool addMe = true;
      cout << entry << endl;

      // Cycle through the interfaces
      BOOST_FOREACH (const pit::IncomingFace &incoming, entry->GetIncoming ())
      {
	// Check if they match to the interface we would like to add
	if (incoming.m_face == n_face)
	  {
	    addMe = false;
	    break;
	  }
      }

      if (addMe) {
	  entry->AddIncoming(n_face);
	  cout << "INFObtained: Adding face " << faceId << endl;
      }
    }

  cout << "Leaving INFObtained" << endl;
}

void
setupRedirection (Ptr<Node> n_node, uint32_t faceId, Time start)
{
  cout << "____________________________________________________________" << endl;
  cout << "Setting up Interest Sector redirection for node " << n_node->GetId () << endl;

  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  Ptr<L3Protocol> protocol = n_node->GetObject <L3Protocol> ();
  Ptr<Face> n_face = protocol->GetFaceById (faceId);

  if (n_face != 0)
    {
      if (stra->m_redirect)
	{
	  cout << "Sector redirection is already on, adding new info" << endl;
	  cout << "Adding Face " << faceId << endl;
	  stra->redirectFaces.insert(n_face);
	} else
	  {
	    cout << "Start: " << start << endl;
	    cout << "Adding Face " << faceId << endl;
	    stra->m_start = start;
	    stra->m_redirect = true;
	    stra->redirectFaces.insert(n_face);
	  }
    }else
      {
	cout << "Got a Null device" << endl;
      }

  cout << "____________________________________________________________" << endl;
}

void
setupDataRedirection (Ptr<Node> n_node, uint32_t faceId, Time start)
{
  cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
  cout << "Setting up Data only redirection for node " << n_node->GetId () << endl;

  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  Ptr<L3Protocol> protocol = n_node->GetObject <L3Protocol> ();
  Ptr<Face> n_face = protocol->GetFaceById(faceId);

  if (n_face != 0)
    {
      if (stra->m_data_redirect)
	{
	  cout << "Data redirection is already on, adding new info" << endl;
	  cout << "Adding Face " << faceId << endl;
	  stra->dataRedirect.insert(n_face);
	} else
	  {

	    cout << "Start: " << start << endl;
	    cout << "Adding Face " << faceId << endl;
	    stra->m_start = start;
	    stra->m_data_redirect = true;
	    stra->dataRedirect.insert(n_face);
	  }
    } else
      {
	cout << "Got a Null device" << endl;
      }
  cout << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
}

void
turnoffDataRedirection (Ptr<Node> n_node)
{
  cout << "------------------------------------------------------------" << endl;
  cout << "Turning off Data redirect for node " << n_node->GetId () <<  " at " << Simulator::Now () << endl;
  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  stra->m_data_redirect = false;
  //stra->dataRedirect.clear();
  cout << "------------------------------------------------------------" << endl;
}

void
turnoffRedirection (Ptr<Node> n_node)
{
  cout << "------------------------------------------------------------" << endl;
  cout << "Turning off sector redirect for node " << n_node->GetId () <<  " at " << Simulator::Now () << endl;
  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  stra->m_redirect = false;
  //stra->redirectFaces.clear();
  cout << "------------------------------------------------------------" << endl;
}

void
setPassthrough (Ptr<Node> n_node)
{
  cout << "------------------------------------------------------------" << endl;
  cout << "Setting pass through for " << n_node->GetId () <<  " at " << Simulator::Now () << endl;
  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  stra->m_passthrough = true;
}

void
turnOffPassthrough (Ptr<Node> n_node)
{
  cout << "------------------------------------------------------------" << endl;
  cout << "Turn off pass through for " << n_node->GetId () <<  " at " << Simulator::Now () << endl;
  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  stra->m_passthrough = false;
}

Ptr<Node>
GetAssociatedNode (NodeContainer nc, Mac48Address mac)
{
  Ptr<Node> n_ret = 0;
  Address cmac = mac.operator ns3::Address();

  for (int i = 0; i < nc.GetN (); i++)
    {
      Ptr<Node> n_tmp = nc.Get (i);

      for (int j = 0; j < n_tmp->GetNDevices(); j++)
	{
	  Ptr<NetDevice> n_dev = n_tmp->GetDevice (j);

	  if (n_dev->GetAddress () == cmac)
	    {
	      n_ret = n_tmp;
	      break;
	    }
	}

      if (n_ret != 0)
	break;
    }

  return n_ret;
}

void
flushNodeBuffer (Ptr<Node> n_node, Ptr<Face> face)
{
  cout << "Flushing buffer of node " << n_node->GetId() << endl;
  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  stra->flushBuffer(face);
}

uint32_t
getNodeBufferSize (Ptr<Node> n_node)
{
  Ptr<fw::SmartFloodingInf> stra = n_node->GetObject <fw::SmartFloodingInf> ();
  return stra->bufferSize();
}

//firstAssociatedPacket(Ptr<const Interest> interest, Ptr<const Face> face)
void
firstAssociatedPacket(Ptr<Node> tmp)
{
  if (readEntry)
    {
      // Check to see if there is anything on the node to push on sector nodes
      switch (numqueue)
      {
	case 1:

	  setPassthrough(tmp);

	  turnoffDataRedirection(tmp);

	  turnOffPassthrough(tmp);

	  turnoffRedirection(NCcenters.Get (0));

	  //turnoffRedirection(NCcenters.Get (0));
	  //turnoffDataRedirection(NCcenters.Get (0));
	  //flushNodeBuffer(NCcenters.Get (0));

	  //tmp = numToNode[1];
	  //turnoffDataRedirection(tmp);
	  //flushNodeBuffer(tmp);

	  break;
	case 2:
	  setPassthrough(NCaps.Get (2));
	  setPassthrough(NCcenters.Get(1));

	  turnoffDataRedirection(NCaps.Get (2));
	  turnoffDataRedirection(NCaps.Get (1));

	  turnOffPassthrough(NCaps.Get (2));
	  turnOffPassthrough(NCcenters.Get(1));

	  turnoffRedirection(NCservers.Get (0));

//	  turnoffRedirection(NCservers.Get (0));
//	  turnoffDataRedirection(NCservers.Get (0));
//	  //flushNodeBuffer(NCservers.Get (0));
//
//	  tmp = NCcenters.Get (1);
//	  turnoffDataRedirection(tmp);
//	  //flushNodeBuffer(tmp);
//
//	  tmp = numToNode[2];
//	  turnoffDataRedirection(tmp);
	  //flushNodeBuffer(tmp);

	  break;
	case 3:

	  setPassthrough(tmp);

	  turnoffDataRedirection(tmp);

	  turnOffPassthrough(tmp);

	  turnoffRedirection(NCcenters.Get (1));

	  //tmp = NCcenters.Get (1);
	  //turnoffDataRedirection(tmp);
	  //flushNodeBuffer(tmp);

	  //tmp = numToNode[3];
	  //turnoffDataRedirection(tmp);
	  //flushNodeBuffer(tmp);
	  break;
      }
      readEntry = false;
    }
}

void
apAssociation (const Mac48Address mac)
{
  Time now = Simulator::Now ();
  Ptr<Node> tmp = GetAssociatedNode(NCaps,mac);

  if (seen_macs.empty()) {
      cout << "============================================================" << endl;
      cout << "Associated to node " <<  tmp->GetId() << " at " << now << endl;
      cout << "First time seeing a MAC Address" << endl;
      // We haven't seen any APs, save
      seen_macs[mac] = tmp;

      mac_queue.push_back(mac);
  } else if (seen_macs.find(mac) != seen_macs.end()) {
  } else {
      cout << "============================================================" << endl;
      cout << "Associated to node " <<  tmp->GetId() << " at " << now << endl;
      cout << "Hit a new MAC, reassociating" << endl;

      // We got something that wasn't in our map, means new AP
      seen_macs[mac] = tmp;

      mac_queue.push_back(mac);
      cout << "Affecting network with REN/INF" << endl;

      readEntry = true;

      Ptr<ForwardingStrategy> fw = tmp->GetObject<ForwardingStrategy> ();

      //fw->TraceConnectWithoutContext ("InInterests", MakeCallback (&firstAssociatedPacket));
      firstAssociatedPacket(tmp);
  }

  if (sectorChange)
    {
      cout << "Executed Sector change" << endl;
      sectorChange = false;
    }
}

void
apDeassociation (const Mac48Address mac)
{
  //cout << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << endl;
  //Time now = Simulator::Now ();
  //Ptr<Node> tmp = GetAssociatedNode(NCaps,mac);
  //cout << "Deassociated from node " << tmp->GetId() << " at " << now << endl;

  //	if (sectorChange) {
  //		cout << "Executing sector change!" << endl;
  //		cout << "Affecting Network with DEN!" << endl;
  //
  //		// Code needed when using one shared channel for all Wifi
  //		switch (numqueue)
  //		{
  //		case 1:
  //			setupRedirection(NCcenters.Get (0), 1, now);
  //			setupDataRedirection(NCaps.Get (1), 1, now);
  //			break;
  //		case 2:
  //			setupRedirection(NCservers.Get (0), 1, now);
  //			setupDataRedirection(NCcenters.Get(1), 0, now);
  //			setupDataRedirection(NCaps.Get (2), 1, now);
  //			break;
  //		case 3:
  //			setupRedirection(NCcenters.Get (1), 1, now);
  //			setupDataRedirection(NCaps.Get (3), 1, now);
  //			break;
  //		}
  //
  //		/////////////////////////////////////////////////////
  //
  //		/////////////////////////////////////////////////////
  //	}
  //cout << "Exiting apDeassociation!" << endl;
  //cout << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << endl;
}

// Function to change the SSID of a Node, depending on distance
void SetSSIDviaDistance(uint32_t mtId, Ptr<MobilityModel> node, std::map<std::string, Ptr<MobilityModel> > aps, bool smartInf)
{
  char configbuf[250];
  char configbuf2[250];
  char buffer[250];

  // This causes the device in mtId to change the SSID, forcing AP change
  sprintf(configbuf, "/NodeList/%d/DeviceList/0/$ns3::WifiNetDevice/Mac/Ssid", mtId);

//  if (smartInf)
//    {
//      sprintf(configbuf2, "/NodeList/%d/DeviceList/0/$ns3::WifiNetDevice/Mac/Ssid", 1);
//    }

  std::map<double, std::string> SsidDistance;

  // Iterate through the map of seen Ssids
  for (std::map<std::string, Ptr<MobilityModel> >::iterator ii=aps.begin(); ii!=aps.end(); ++ii)
    {
      // Calculate the distance from the AP to the node and save into the map
      SsidDistance[node->GetDistanceFrom((*ii).second)] = (*ii).first;
    }

  double distance = SsidDistance.begin()->first;
  std::string ssid(SsidDistance.begin()->second);

  // If the first time, no sector change
  if (ssidOld.empty())
    {
      ssidOld = ssid;
    }
  else
    {
      if (ssidOld.compare(ssid) != 0)
	{
	  sprintf(buffer, "We will have sector change from %s to %s", ssidOld.c_str(), ssid.c_str());
	  NS_LOG_INFO(buffer);

	  ssidOld = ssid;
	  sectorChange = true;
	  if (smartInf)
	    {

	      numqueue = ssidToNum[ssid];
	      if (numqueue == 1)
		{
		  // Central node
		  setupRedirection(NCcenters.Get (0), 1, Simulator::Now());
		  //setupDataRedirection(NCcenters.Get (0), 1, Simulator::Now());

		  // AP node
		  setupDataRedirection(NCaps.Get (1), 1, Simulator::Now());
		}

	      if (numqueue == 2)
		{
		  // Schedule changes
		  // Server
		  setupRedirection(NCservers.Get (0), 1, Simulator::Now());
		  //setupDataRedirection(NCservers.Get (0), 1, Simulator::Now());

		  // Central node
		  setupDataRedirection(NCcenters.Get (1), 0, Simulator::Now());

		  // Ap node
		  setupDataRedirection(NCaps.Get (2), 1, Simulator::Now());
		}

	      if (numqueue == 3)
		{
		  // Central node
		  setupRedirection(NCcenters.Get (1), 1, Simulator::Now());
		  //setupDataRedirection(NCcenters.Get (1), 1, Simulator::Now());

		  // Ap node
		  setupDataRedirection(NCaps.Get (3), 1, Simulator::Now());
		}
	    }
	}
    }

  sprintf(buffer, "Change to SSID %s at distance of %f", ssid.c_str(), distance);

  NS_LOG_INFO(buffer);
  NS_LOG_INFO("Change at " << Simulator::Now() );

  // Because the map sorts by std:less, the first position has the lowest distance
  Config::Set(configbuf, SsidValue(ssid));

//  if (smartInf)
//    {
//      Config::Set(configbuf2, SsidValue(ssid));
//    }

  // Empty the maps
  SsidDistance.clear();
}

int main (int argc, char *argv[])
{
  // These are our scenario arguments
  uint32_t sectors = 2;                         // Number of wireless sectors
  uint32_t aps = 2;					          // Number of wireless access nodes in a sector
  uint32_t mobile = 1;				          // Number of mobile terminals
  uint32_t servers = 1;				          // Number of servers in the network
  uint32_t wnodes = aps * sectors;              // Number of nodes in the network
  uint32_t xaxis = 300;                         // Size of the X axis
  uint32_t yaxis = 300;                         // Size of the Y axis
  double sec = 0.0;                             // Movement start
  bool fake = false;							  // Enable fake interest or not
  bool traceFiles = false;                      // Tells to run the simulation with traceFiles
  bool smart = false;                           // Tells to run the simulation with SmartFlooding
  bool bestr = false;                           // Tells to run the simulation with BestRoute
  bool smartInf = false;                        // Tells to run the simulation with SmartFlooding INF style
  bool walk = true;                             // Do random walk at walking speed
  double speed= 5;							  // MN's speed	change here (1.4 | 8.3 | 16.7)
  bool wifig = false;
  char results[250] = "results";                // Directory to place results
  double endTime = 200;                         // Number of seconds to run the simulation
  double MBps = 0.151552;                       // MB/s data rate desired for applications
  int contentSize = -1;                         // Size of content to be retrieved
  int maxSeq = -1;                              // Maximum number of Data packets to request
  double retxtime = 0.05;                       // How frequent Interest retransmission timeouts should be checked (seconds)
  int csSize = 10000000;                        // How big the Content Store should be
  //double deltaTime = 10;
  std::string nsTFile;                          // Name of the NS Trace file to use
  char nsTDir[250] = "./Waypoints";           // Directory for the waypoint files

  // Variable for buffer
  char buffer[250];

  CommandLine cmd;
  cmd.AddValue ("mobile", "Number of mobile terminals in simulation", mobile);
  cmd.AddValue ("servers", "Number of servers in the simulation", servers);
  cmd.AddValue ("results", "Directory to place results", results);
  cmd.AddValue ("start", "Starting second", sec);
  cmd.AddValue ("fake", "Enable fake interest", fake);
  cmd.AddValue ("trace", "Enable trace files", traceFiles);
  cmd.AddValue ("smart", "Enable SmartFlooding forwarding", smart);
  cmd.AddValue ("sinf", "Enable SmartFlooding with INF", smartInf);
  cmd.AddValue ("bestr", "Enable BestRoute forwarding", bestr);
  cmd.AddValue ("csSize", "Number of Interests a Content Store can maintain", csSize);
  cmd.AddValue ("walk", "Enable random walk at walking speed", walk);
  cmd.AddValue ("speed", "Number of speed/hour of mobile terminals in the simulation", speed);
  cmd.AddValue ("endTime", "How long the simulation will last (Seconds)", endTime);
  cmd.AddValue ("mbps", "Data transmission rate for NDN App in MBps", MBps);
  cmd.AddValue ("size", "Content size in MB (-1 is for no limit)", contentSize);
  cmd.AddValue ("retx", "How frequent Interest retransmission timeouts should be checked in seconds", retxtime);
  cmd.AddValue ("wifig", "Use Wifi G Standard", wifig);
  cmd.AddValue ("traceFile", "Directory containing Ns2 movement trace files (Usually created by Bonnmotion)", nsTDir);
  //cmd.AddValue ("deltaTime", "time interval (s) between updates (default 100)", deltaTime);
  cmd.Parse (argc,argv);

  NS_LOG_INFO("Random walk at human walking speed - 1.4m/s");
  sprintf(buffer, "ns3::ConstantRandomVariable[Constant=%f]", speed);

  uint32_t top = speed;

  double realspeed = 0.0;

  sprintf(buffer, "%s/Walk_random.ns_movements", nsTDir);
  switch (top)
  {
    case 5:
      sprintf(buffer, "%s/1.4.ns_movements", nsTDir);
      realspeed = 1.4;
      break;
    case 10:
      sprintf(buffer, "%s/2.8.ns_movements", nsTDir);
      realspeed = 2.8;
      break;
    case 20:
      sprintf(buffer, "%s/5.6.ns_movements", nsTDir);
      realspeed = 5.6;
      break;
    case 30:
      sprintf(buffer, "%s/8.3.ns_movements", nsTDir);
      realspeed = 8.3;
      break;
    case 40:
      sprintf(buffer, "%s/11.2.ns_movements", nsTDir);
      realspeed = 11.2;
      break;
    case 50:
      sprintf(buffer, "%s/13.9.ns_movements", nsTDir);
      realspeed = 13.9;
      break;
    case 60:
      sprintf(buffer, "%s/16.7.ns_movements", nsTDir);
      realspeed = 16.7;
      break;
    case 70:
      sprintf(buffer, "%s/19.4.ns_movements", nsTDir);
      realspeed = 19.4;
      break;
    case 80:
      sprintf(buffer, "%s/22.2.ns_movements", nsTDir);
      realspeed = 22.2;
      break;
  }
  nsTFile = buffer;
  endTime = round(400 / realspeed);
  cout << "endtime=" << endTime << endl;


  vector<double> centralXpos;
  vector<double> centralYpos;
  centralXpos.push_back(50.0);
  centralXpos.push_back(250.0);
  centralYpos.push_back(-50.0);
  centralYpos.push_back(-50.0);

  vector<double> wirelessXpos;
  vector<double> wirelessYpos;
  wirelessXpos.push_back(0);
  wirelessXpos.push_back(100);
  wirelessXpos.push_back(200);
  wirelessXpos.push_back(300);
  wirelessYpos.push_back(0);
  wirelessYpos.push_back(0);
  wirelessYpos.push_back(0);
  wirelessYpos.push_back(0);

  // What the NDN Data packet payload size is fixed to 1024 bytes
  uint32_t payLoadsize = 1024;

  // Give the content size, find out how many sequence numbers are necessary
  if (contentSize > 0)
    {
      maxSeq = 1 + (((contentSize*1000000) - 1) / payLoadsize);
    }

  // How many Interests/second a producer creates
  double intFreq = (MBps * 1000000) / payLoadsize;

  NS_LOG_INFO ("------Creating nodes------");
  // Node definitions for mobile terminals (consumers)
  NodeContainer mobileTerminalContainer;
  mobileTerminalContainer.Create(mobile);

  std::vector<uint32_t> mobileNodeIds;

  NS_LOG_INFO("------ Mobile Ids ------");

  // Save all the mobile Node IDs
  for (int i = 0; i < mobile; i++)
    {
      mobileNodeIds.push_back(mobileTerminalContainer.Get (i)->GetId ());
      NS_LOG_INFO(mobileTerminalContainer.Get (i)->GetId ());
    }

  // Central Nodes
  NodeContainer centralContainer;
  centralContainer.Create (sectors);

  std::vector<uint32_t> centralNodesIds;

  NS_LOG_INFO("------ Central Ids ------");
  // Save all the mobile Node IDs
  for (int i = 0; i < sectors; i++)
    {
      centralNodesIds.push_back(centralContainer.Get (i)->GetId ());
      NS_LOG_INFO(centralContainer.Get (i)->GetId ());
    }

  // Wireless access Nodes
  NodeContainer wirelessContainer;
  wirelessContainer.Create (wnodes);

  std::vector<uint32_t> wirelessNodesIds;

  NS_LOG_INFO("------ Wireless Ids ------");
  for (int i = 0; i < wnodes; i++)
    {
      wirelessNodesIds.push_back(wirelessContainer.Get (i)->GetId ());
      NS_LOG_INFO(wirelessContainer.Get (i)->GetId ());
    }

  // Separate the wireless nodes into sector specific containers
  std::vector<NodeContainer> sectorNodes;

  for (int i = 0; i < sectors; i++)
    {
      NodeContainer wireless;
      for (int j = i*aps; j < aps + i*aps; j++)
	{
	  wireless.Add(wirelessContainer.Get (j));
	}
      sectorNodes.push_back(wireless);
    }

  // Find out how many first level nodes we will have
  // The +1 is for the server which will be attached to the first level nodes
  int first = (sectors / 3) + 1;


  // Container for all NDN capable nodes
  NodeContainer allNdnNodes;
  allNdnNodes.Add (centralContainer);
  allNdnNodes.Add (wirelessContainer);

  // Container for server (producer) nodes
  NodeContainer serverNodes;
  serverNodes.Create (servers);

  std::vector<uint32_t> serverNodeIds;

  NS_LOG_INFO("------ Server Ids ------");
  // Save all the mobile Node IDs
  for (int i = 0; i < servers; i++)
    {
      serverNodeIds.push_back(serverNodes.Get (i)->GetId ());
      NS_LOG_INFO(serverNodes.Get(i)->GetId ());
    }

  // Container for all nodes without NDN specific capabilities
  NodeContainer allUserNodes;
  allUserNodes.Add (mobileTerminalContainer);
  //allUserNodes.Add (serverNodes);

  allNdnNodes.Add (serverNodes);

  NCaps = wirelessContainer;
  NCcenters = centralContainer;
  NCmobiles = mobileTerminalContainer;
  NCservers = serverNodes;

  // Container for each AP to be identified
  NodeContainer SSID1;
  SSID1.Add (wirelessContainer.Get(0));
  NodeContainer SSID2;
  SSID2.Add (wirelessContainer.Get(1));
  NodeContainer SSID3;
  SSID3.Add (wirelessContainer.Get(2));
  NodeContainer SSID4;
  SSID4.Add (wirelessContainer.Get(3));

  // Make sure to seed our random
  gen.seed (std::time (0) + (long long)getpid () << 32);

  MobilityHelper Server;
  Ptr<ListPositionAllocator> initialServer = CreateObject<ListPositionAllocator> ();

  Vector posServer (150, -100, 0.0);
  initialServer->Add (posServer);

  Server.SetPositionAllocator(initialServer);
  Server.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  Server.Install(serverNodes);

  NS_LOG_INFO ("------Placing Central nodes-------");
  MobilityHelper centralStations;

  Ptr<ListPositionAllocator> initialCenter = CreateObject<ListPositionAllocator> ();

  for (int i = 0; i < sectors; i++)
    {
      Vector pos (centralXpos[i], centralYpos[i], 0.0);
      initialCenter->Add (pos);
    }

  centralStations.SetPositionAllocator(initialCenter);
  centralStations.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  centralStations.Install(centralContainer);

  NS_LOG_INFO ("------Placing wireless access nodes------");
  MobilityHelper wirelessStations;

  Ptr<ListPositionAllocator> initialWireless = CreateObject<ListPositionAllocator> ();

  for (int i = 0; i < wnodes; i++)
    {
      Vector pos (wirelessXpos[i], wirelessYpos[i], 0.0);
      initialWireless->Add (pos);
    }

  wirelessStations.SetPositionAllocator(initialWireless);
  wirelessStations.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  wirelessStations.Install(wirelessContainer);


  NS_LOG_INFO ("------Placing mobile node and determining direction and speed------");
  MobilityHelper mobileStations;


  sprintf(buffer, "0|%d|0|%d", xaxis, yaxis);
  string bounds = string(buffer);


  sprintf(buffer, "Reading NS trace file %s", nsTFile.c_str());
  NS_LOG_INFO(buffer);

  Ns2MobilityHelper ns2 = Ns2MobilityHelper (nsTFile);
  ns2.Install ();

//  MobilityHelper mobile2;
//  Ptr<ListPositionAllocator> initialMobile2 = CreateObject<ListPositionAllocator> ();
//
//  Vector posMobile2 (150, 0, 0.0);
//  initialMobile2->Add (posMobile2);
//
//  mobile2.SetPositionAllocator(initialMobile2);
//  mobile2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
//  mobile2.Install(mobileTerminalContainer.Get (1));

  // Connect Wireless Nodes to central nodes
  // Because the simulation is using Wifi, PtP connections are 100Mbps
  // with 5ms delay
  NS_LOG_INFO("------Connecting Central nodes to wireless access nodes------");

  vector <NetDeviceContainer> ptpWLANCenterDevices;

  PointToPointHelper p2p_100mbps5ms;
  p2p_100mbps5ms.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p_100mbps5ms.SetChannelAttribute ("Delay", StringValue ("1ms"));

  for (int i = 0; i < sectors; i++)
    {
      NetDeviceContainer ptpWirelessCenterDevices;

      for (int j = 0; j < aps; j++)
	{
	  ptpWirelessCenterDevices.Add (p2p_100mbps5ms.Install (centralContainer.Get (i), sectorNodes[i].Get (j) ));
	}

      ptpWLANCenterDevices.push_back (ptpWirelessCenterDevices);
    }

  // Connect the server to central node
  NetDeviceContainer ptpServerlowerNdnDevices;
  for(int i =0; i < sectors; i++){
      ptpServerlowerNdnDevices.Add (p2p_100mbps5ms.Install (serverNodes.Get (0), centralContainer.Get (i)));

  }

  NS_LOG_INFO ("------Creating Wireless cards------");

  // Use the Wifi Helper to define the wireless interfaces for APs
  WifiHelper wifi;
  if (wifig)
    {
      wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    }
  // The N standard is apparently not completely supported in NS-3
  //wifi.setStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
  // The ConstantRateWifiManager only works with one rate, making issues
  //wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
  // The MinstrelWifiManager isn't working on the current version of NS-3
  //wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager");
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::ThreeLogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  /////////////////////////////////////////////////////

  // All interfaces are placed on the same channel. Makes AP changes easy. Might
  // have to be reconsidered for multiple mobile nodes
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
  wifiPhyHelper.SetChannel (wifiChannel.Create());
  wifiPhyHelper.Set("TxPowerStart", DoubleValue(16.0206));
  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(16.0206));

  // Add a simple no QoS based card to the Wifi interfaces
  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();

  // Create SSIDs for all the APs
  std::vector<Ssid> ssidV;

  NS_LOG_INFO ("------Creating ssids for wireless cards------");

  // We store the Wifi AP mobility models in a map, ordered by the ssid string. Will be easier to manage when
  // calling the modified StaMApWifiMac
  std::map<std::string, Ptr<MobilityModel> > apTerminalMobility;

  for (int i = 0; i < wnodes +1; i++)
    {
      // Temporary string containing our SSID
      std::string ssidtmp("ap-" + boost::lexical_cast<std::string>(i));

      // Push the newly created SSID into a vector
      ssidV.push_back (Ssid (ssidtmp));

      ssidToNode[ssidtmp] = wirelessContainer.Get (i);
      ssidToNum[ssidtmp] = i;
      numToNode[i] = wirelessContainer.Get (i);

      if (i < wnodes) {
	  // Get the mobility model for wnode i
	  Ptr<MobilityModel> tmp = (wirelessContainer.Get (i))->GetObject<MobilityModel> ();

	  // Store the information into our map
	  apTerminalMobility[ssidtmp] = tmp;
      }
    }

  NS_LOG_INFO ("------Assigning mobile terminal wireless cards------");

  NS_LOG_INFO ("Assigning AP wireless cards");
  std::vector<NetDeviceContainer> wifiAPNetDevices;

  for (int i = 0; i < wnodes; i++)
    {
      wifiMacHelper.SetType ("ns3::ApWifiMac",
			     "Ssid", SsidValue (ssidV[i]),
			     "BeaconGeneration", BooleanValue (true),
			     "BeaconInterval", TimeValue (Seconds (0.1)));

      /////////////////////////////////////////////////////

      wifiAPNetDevices.push_back (wifi.Install (wifiPhyHelper, wifiMacHelper, wirelessContainer.Get (i)));
    }

  // Create a Wifi station with a modified Station MAC.
  wifiMacHelper.SetType("ns3::StaWifiMac",
			"Ssid", SsidValue (ssidV[wnodes]),
			"ActiveProbing", BooleanValue (true));


  std::vector<NetDeviceContainer> mobileDevices;

  /////////////////////////////////////////////////////

  mobileDevices.push_back(wifi.Install(wifiPhyHelper, wifiMacHelper, mobileTerminalContainer.Get (0)));

  //mobileDevices.push_back(wifi.Install(wifiPhyHelper, wifiMacHelper, mobileTerminalContainer.Get (1)));

  // Using the same calculation from the Yans-wifi-Channel, we obtain the Mobility Models for the
  // mobile node as well as all the Wifi capable nodes
  Ptr<MobilityModel> mobileTerminalMobility = (mobileTerminalContainer.Get (0))->GetObject<MobilityModel> ();

  std::vector<Ptr<MobilityModel> > mobileTerminalsMobility;

  // Get the list of mobile node mobility models
  for (int i = 0; i < mobile; i++)
    {
      mobileTerminalsMobility.push_back((mobileTerminalContainer.Get (i))->GetObject<MobilityModel> ());
    }

  char routeType[250];

  // Now install content stores and the rest on the middle node. Leave
  // out clients and the mobile node
  NS_LOG_INFO ("------Installing NDN stack on routers------");
  ndn::StackHelper ndnHelperRouters;

  // Decide what Forwarding strategy to use depending on user command line input
  if (smart) {
      sprintf(routeType, "%s", "smart");
      NS_LOG_INFO ("NDN Utilizing SmartFlooding");
      ndnHelperRouters.SetForwardingStrategy ("ns3::ndn::fw::SmartFlooding::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Window");
  } else if (bestr) {
      sprintf(routeType, "%s", "bestr");
      NS_LOG_INFO ("NDN Utilizing BestRoute");
      ndnHelperRouters.SetForwardingStrategy ("ns3::ndn::fw::BestRoute::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Window");
  } else if (smartInf) {
      sprintf(routeType, "%s", "smartinf");
      NS_LOG_INFO ("NDN Utilizing SmartFlooding with INF");
      ndnHelperRouters.SetForwardingStrategy ("ns3::ndn::fw::SmartFloodingInf");
  } else {
      sprintf(routeType, "%s", "flood");
      NS_LOG_INFO ("NDN Utilizing Flooding");
      ndnHelperRouters.SetForwardingStrategy ("ns3::ndn::fw::Flooding::PerOutFaceLimits", "Limit", "ns3::ndn::Limits::Window");
  }

  // Set the Content Stores

  sprintf(buffer, "%d", csSize);

  ndnHelperRouters.SetContentStore ("ns3::ndn::cs::Freshness::Lru", "MaxSize", buffer);
  ndnHelperRouters.SetDefaultRoutes (true);
  // Install on ICN capable routers
  ndnHelperRouters.Install (allNdnNodes);

  // We have to tell which nodes are edges
  if (smartInf)
    {
      for (int i = 0; i < wirelessContainer.GetN(); i++)
	{
	  Ptr<fw::SmartFloodingInf> stra = wirelessContainer.Get(i)->GetObject <fw::SmartFloodingInf> ();
	  stra->m_edge = true;

	}

      for (int i = 0; i < allNdnNodes.GetN (); i++)
	{
	  Ptr<fw::SmartFloodingInf> stra = allNdnNodes.Get(i)->GetObject <fw::SmartFloodingInf> ();
	  stra->m_rtx = Seconds(retxtime);
	}
    }

  // Create a NDN stack for the clients and mobile node
  ndn::StackHelper ndnHelperUsers;
  // These nodes have only one interface, so BestRoute forwarding makes sense
  ndnHelperUsers.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");
  // No Content Stores are installed on these machines
  ndnHelperUsers.SetContentStore ("ns3::ndn::cs::Nocache");
  ndnHelperUsers.SetDefaultRoutes (true);
  ndnHelperUsers.Install (allUserNodes);

  NS_LOG_INFO ("------Installing Producer Application------");

  sprintf(buffer, "Producer Payload size: %d", payLoadsize);
  NS_LOG_INFO (buffer);

  // Create the producer on the mobile node
  ndn::AppHelper producerHelper ("ns3::ndn::Producer");
  producerHelper.SetPrefix ("/waseda/sato");
  producerHelper.SetAttribute ("StopTime", TimeValue (Seconds(endTime)));
  // Payload size is in bytes
  producerHelper.SetAttribute ("PayloadSize", UintegerValue(payLoadsize));
  producerHelper.Install (serverNodes);

  NS_LOG_INFO ("------Installing Consumer Application------");

  sprintf(buffer, "Consumer Interest/s frequency: %f", intFreq);
  NS_LOG_INFO (buffer);

  sprintf(buffer, "Consumer retransmission timer: %f", retxtime);
  NS_LOG_INFO (buffer);

  // Create the consumer on the randomly selected node
  ndn::AppHelper consumerHelper ("ns3::ndn::PriConsumer");
  consumerHelper.SetPrefix ("/waseda/sato");
  consumerHelper.SetAttribute ("Frequency", DoubleValue (intFreq));
  consumerHelper.SetAttribute ("StartTime", TimeValue (Seconds(1)));
  consumerHelper.SetAttribute ("StopTime", TimeValue (Seconds (endTime+5)));
  consumerHelper.SetAttribute ("RetxTimer", TimeValue (Seconds(retxtime)));
  if (maxSeq > 0)
    consumerHelper.SetAttribute ("MaxSeq", IntegerValue(maxSeq));

  consumerHelper.Install (mobileTerminalContainer);
  if(fake)	consumerHelper.Install (centralContainer);			//change here (normal / fake interest)

  sprintf(buffer, "Ending time! %f", endTime+5);
  NS_LOG_INFO(buffer);



  // If the variable is set, print the trace files
  if (traceFiles) {
      // Filename
      char filename[250];

      // File ID
      char fileId[250];


      // Create the file identifier
      sprintf(fileId, "%s-%02d-%03d-%03d.txt", routeType, mobile, servers, wnodes);

      /*		sprintf(filename, "%s/%s/%s/%.0f/clients", results, scenario, speed);

		std::ofstream clientFile;

		clientFile.open (filename);
		for (int i = 0; i < mobileNodeIds.size(); i++)
		{
			clientFile << mobileNodeIds[i] << std::endl;
		}

		clientFile.close();

		// Print server nodes to file
		sprintf(filename, "%s/%s/%s/%.0f/servers", results, scenario, speed);

		std::ofstream serverFile;

		serverFile.open (filename);
		for (int i = 0; i < serverNodeIds.size(); i++)
		{
			serverFile << serverNodeIds[i] << std::endl;
		}

		serverFile.close();
       */
      char mode[7];
      if(fake) sprintf(mode, "fake");
      else sprintf(mode, "normal");

      NS_LOG_INFO ("Installing tracers");

      int text = retxtime*1000;

      // NDN Aggregate tracer
      printf ("now I'm writing the files at %s/%s/%s/%.0f/\n", results, scenario, mode, speed);
      sprintf (filename, "%s/%s/%s/%.0f/aggregate-trace-%s-%d", results, scenario, mode, speed, routeType, text);
      ndn::L3AggregateTracer::InstallAll(filename, Seconds (1.0));

      // NDN L3 tracer
      sprintf (filename, "%s/%s/%s/%.0f/rate-trace-%s-%d", results, scenario, mode, speed, routeType, text);
      ndn::L3RateTracer::InstallAll (filename, Seconds (1.0));

      // NDN App Tracer
      sprintf (filename, "%s/%s/%s/%.0f/app-delays-%s-%d", results, scenario, mode, speed, routeType, text);
      ndn::AppDelayTracer::InstallAll (filename);

      // L2 Drop rate tracer
      //		sprintf (filename, "%s/%s/%s/%.0f/drop-trace", results, scenario, mode, speed);
      //		L2RateTracer::InstallAll (filename, Seconds (0.5));

      // Content Store tracer
      //		sprintf (filename, "%s/%s/%s/%.0f/cs-trace", results, scenario, mode, speed);
      //		ndn::CsTracer::InstallAll (filename, Seconds (1));
  }

  // Get the Consumer application
  Ptr<PriConsumer> consumer = DynamicCast<PriConsumer> (mobileTerminalContainer.Get (0)->GetApplication(0));

  NS_LOG_INFO ("------Scheduling events - SSID changes------");

  char configbuf[250];

  for (int i = 0; i < mobileTerminalContainer.GetN (); i++)
    {

      if (smartInf)
	{
	  // When associating
	  sprintf(configbuf, "/NodeList/%d/DeviceList/%d/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", mobileTerminalContainer.Get (i)->GetId(), 0);
	  // Connect to the tracing
	  Config::ConnectWithoutContext(configbuf, MakeCallback(&apAssociation));

	  // When disassociating
	  sprintf(configbuf, "/NodeList/%d/DeviceList/%d/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc", mobileTerminalContainer.Get (i)->GetId(), 0);
	  // Connect to the tracing
	  Config::ConnectWithoutContext(configbuf, MakeCallback(&apDeassociation));
	}
    }

  // Schedule AP Changes
  double apsec = 0.0;
  // How often should the AP check it's distance
  double checkTime = 100.0/realspeed;
  // How often should we check the timeouts (milliseconds)
  double totalCheckTime = 1000;
  double timeTime = 5;
  double tmpT = 0;

  // Stop the application from generating more things without actually dying
  Simulator::Schedule(Seconds(endTime), &Config::Set, "/NodeList/"+boost::lexical_cast<string> (mobileTerminalContainer.Get (0)->GetId())+"/ApplicationList/*/$ns3::ndn::ConsumerCbr/Frequency",
               DoubleValue(0.1));

  double j = apsec;
  int k = 0;

  while ( j < endTime)
    {
      for (int i = 0; i < 1; i++)
	{
	  sprintf(buffer, "Running event SSID distance event at %f for node ", j);
	  NS_LOG_INFO(buffer);

	  uint32_t nodeId = mobileTerminalContainer[i]->GetId();
	  Simulator::Schedule (Seconds(j), &SetSSIDviaDistance, nodeId, mobileTerminalsMobility[i], apTerminalMobility, smartInf);
	}


      //		Time torun = Seconds(j);
      //		Time denPush = torun - MilliSeconds(10);


      //		for (int i = 0; i < mobile && k <= 3; i++)
      //		{
      //			Time ssi = torun;
      //
      //			cout << "Scheduling SSID change for mobile node " << i << " at " << torun << endl;
      //
      //			/////////////////////////////////////////////////////
      //
      //			/////////////////////////////////////////////////////
      ////
      ////			if (k > 0)
      ////			{
      ////				ssi -= MilliSeconds(10);
      ////				Simulator::Schedule(torun, &SetSSID, mobileNodeIds[i], k-1, ssidV[wnodes]);
      ////			}
      ////			Simulator::Schedule (ssi, &SetSSID, mobileNodeIds[i], k, ssidV[k]);
      //
      //			/////////////////////////////////////////////////////
      //			Simulator::Schedule (ssi, &SetSSID, mobileNodeIds[i], 0, ssidV[k]);
      //
      //		}

      //		if (smartInf) {
      //
      //			Ptr<Node> tmp;
      //			Ptr<Node> tmp2;
      //			if (k == 1)
      //			{
      //				cout << "K = 1 Scheduling for " << denPush << endl;
      //				// Central node
      //				Simulator::Schedule (denPush, &setupRedirection, NCcenters.Get (0), 1, torun);
      //				// AP node
      //				Simulator::Schedule (denPush, &setupDataRedirection, NCaps.Get (1), 1, torun);
      //			}
      //
      //			if (k == 2)
      //			{
      //				cout << "K = 2 Scheduling for " <<  denPush << endl;
      //				// Schedule changes
      //				// Server
      //				Simulator::Schedule (denPush, &setupRedirection, serverNodes.Get (0), 1, torun);
      //
      //				// Central node
      //				Simulator::Schedule (denPush, &setupDataRedirection, centralContainer.Get (1), 0, torun);
      //
      //				// Ap node
      //				Simulator::Schedule (denPush, &setupDataRedirection, wirelessContainer.Get (2), 1, torun);
      //			}
      //
      //			if (k == 3)
      //			{
      //				cout << "K = 3 Scheduling for " <<  denPush << endl;
      //				// Central node
      //				Simulator::Schedule (denPush, &setupRedirection, centralContainer.Get (1), 1, torun);
      //
      //				// Ap node
      //				Simulator::Schedule (denPush, &setupDataRedirection, wirelessContainer.Get (3), 1, torun);
      //			}
      //		}

      //		while (tmpT <= totalCheckTime && k > 0 && k <= 3)
      //		{
      //			Time tosche = torun + MilliSeconds(tmpT);
      //			cout << "Running packet sequence event at " << tosche << endl;
      //			//Simulator::Schedule (tosche, &PrintSeqs, consumer);
      //			Simulator::Schedule (tosche, &INFObtained, centralContainer.Get (1), 2);
      //			tmpT += timeTime;
      //		}
      //
      //		NS_LOG_INFO ("------Testing PIT printing------");
      //Simulator::Schedule (Seconds(j), &PeriodicPitPrinter, wirelessContainer.Get(0));
      //	    Simulator::Schedule (Seconds(j), &PITEntryCreator, wirelessContainer.Get(k), wirelessContainer.Get(k+1));
      j += checkTime;
      tmpT = 0;
      k++;
    }

  NS_LOG_INFO ("------Ready for execution!------");

  Simulator::Stop (Seconds (endTime+5));
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_INFO ("End");
}
