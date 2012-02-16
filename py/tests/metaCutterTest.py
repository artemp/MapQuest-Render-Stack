#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Test meta data cutter
#
#  Author: kevin.kreiser@mapquest.com
#
#  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#------------------------------------------------------------------------------

import unittest
import logging
import json
from mapnik2 import Box2d

import sys
sys.path.append( "../" )
import metacutter

#for mocking a mapnik inmem meta writer return
class mnkInmem:

	def __init__(self,box,**kwargs):
		self.box = box
		self.properties = [(k,v) for k,v in kwargs.iteritems()]
		


#for testing the mapware renderer
class testMetaCutter(unittest.TestCase):

	#some setup for the tests
	def setUp(self):
		logging.basicConfig(level=logging.DEBUG)

	#test a mapware metadata return
	def test_mapware_meta(self):
		#this is what the data looks like that comes back tacked on to the end of the mapware image
		data = '{"pois":[{"name":"Meadowlands Sports Complex Station","id":"269623123","icon":{"x1":621,"y1":1270,"x2":634,"y2":1291},"label":{"x1":634,"y1":1250,"x2":734,"y2":1270}},{"name":"Teterboro Station","id":"269623198","icon":{"x1":673,"y1":874,"x2":686,"y2":895},"label":{"x1":686,"y1":864,"x2":768,"y2":874}},{"name":"Wood Ridge Station","id":"269623214","icon":{"x1":581,"y1":1033,"x2":594,"y2":1054},"label":{"x1":594,"y1":1023,"x2":687,"y2":1033}},{"name":"2nd Street Station","id":"269621945","icon":{"x1":790,"y1":1822,"x2":803,"y2":1843},"label":{"x1":803,"y1":1812,"x2":888,"y2":1822}},{"name":"Exchange Place Station","id":"269621952","icon":{"x1":840,"y1":2020,"x2":853,"y2":2041},"label":{"x1":853,"y1":2010,"x2":966,"y2":2020}},{"name":"Harsimus Cove Station","id":"269621955","icon":{"x1":822,"y1":1969,"x2":835,"y2":1990},"label":{"x1":835,"y1":1959,"x2":945,"y2":1969}},{"name":"Forest Hills Station","id":"269622000","icon":{"x1":1943,"y1":1993,"x2":1956,"y2":2014},"label":{"x1":1851,"y1":1983,"x2":1943,"y2":1993}},{"name":"Riverfront Stadium Station","id":"269622404","icon":{"x1":51,"y1":1790,"x2":64,"y2":1811},"label":{"x1":64,"y1":1780,"x2":190,"y2":1790}},{"name":"Bronxville Station","id":"269622466","icon":{"x1":2000,"y1":286,"x2":2013,"y2":307},"label":{"x1":1918,"y1":276,"x2":2000,"y2":286}},{"name":"Fleetwood Station","id":"269622482","icon":{"x1":1970,"y1":399,"x2":1983,"y2":420},"label":{"x1":1887,"y1":389,"x2":1970,"y2":399}},{"name":"Glenwood Station","id":"269622486","icon":{"x1":1629,"y1":207,"x2":1642,"y2":228},"label":{"x1":1642,"y1":197,"x2":1724,"y2":207}},{"name":"Greystone Station","id":"269622491","icon":{"x1":1684,"y1":42,"x2":1697,"y2":63},"label":{"x1":1697,"y1":32,"x2":1782,"y2":42}},{"name":"Yonkers Station","id":"269622562","icon":{"x1":1610,"y1":321,"x2":1623,"y2":342},"label":{"x1":1623,"y1":311,"x2":1698,"y2":321}},{"name":"Military Park Station","id":"269622568","icon":{"x1":44,"y1":1855,"x2":57,"y2":1876},"label":{"x1":57,"y1":1845,"x2":154,"y2":1855}},{"name":"Branch Brook Park Station","id":"269622565","icon":{"x1":21,"y1":1531,"x2":34,"y2":1552},"label":{"x1":34,"y1":1521,"x2":156,"y2":1531}},{"name":"Newark Penn Station","id":"269622570","icon":{"x1":84,"y1":1877,"x2":97,"y2":1898},"label":{"x1":97,"y1":1867,"x2":197,"y2":1877}},{"name":"75th Avenue Station","id":"269622708","icon":{"x1":1989,"y1":2003,"x2":2002,"y2":2024},"label":{"x1":1895,"y1":2024,"x2":1989,"y2":2034}},{"name":"Bedford Avenue Station","id":"269622769","icon":{"x1":1289,"y1":2009,"x2":1302,"y2":2030},"label":{"x1":1302,"y1":1999,"x2":1413,"y2":2009}},{"name":"Bowery Station","id":"269622780","icon":{"x1":1074,"y1":1987,"x2":1087,"y2":2008},"label":{"x1":1087,"y1":1977,"x2":1156,"y2":1987}},{"name":"Canal Street (1 Station","id":"269622799","icon":{"x1":1002,"y1":1970,"x2":1015,"y2":1991},"label":{"x1":893,"y1":1991,"x2":1002,"y2":2001}},{"name":"Canal Street (4 Station","id":"269622800","icon":{"x1":1037,"y1":2000,"x2":1050,"y2":2021},"label":{"x1":1050,"y1":2021,"x2":1158,"y2":2031}},{"name":"Houston Street Station","id":"269622892","icon":{"x1":1008,"y1":1923,"x2":1021,"y2":1944},"label":{"x1":901,"y1":1913,"x2":1008,"y2":1923}},{"name":"Nassau Avenue Station","id":"269622950","icon":{"x1":1323,"y1":1954,"x2":1336,"y2":1975},"label":{"x1":1336,"y1":1944,"x2":1449,"y2":1954}},{"name":"Prince Street Station","id":"269622980","icon":{"x1":1052,"y1":1957,"x2":1065,"y2":1978},"label":{"x1":953,"y1":1947,"x2":1052,"y2":1957}},{"name":"Garfield Station","id":"269623020","icon":{"x1":424,"y1":861,"x2":437,"y2":882},"label":{"x1":437,"y1":851,"x2":512,"y2":861}},{"name":"Broadway Station","id":"269623069","icon":{"x1":368,"y1":432,"x2":381,"y2":453},"label":{"x1":381,"y1":422,"x2":462,"y2":432}},{"name":"Clifton Station","id":"269623073","icon":{"x1":146,"y1":851,"x2":159,"y2":872},"label":{"x1":159,"y1":841,"x2":225,"y2":851}},{"name":"Delawanna Station","id":"269623076","icon":{"x1":274,"y1":1133,"x2":287,"y2":1154},"label":{"x1":287,"y1":1123,"x2":376,"y2":1133}},{"name":"Emerson Station","id":"269623086","icon":{"x1":879,"y1":25,"x2":892,"y2":46},"label":{"x1":892,"y1":15,"x2":971,"y2":25}},{"name":"Glen Rock Main Line Station","id":"269623094","icon":{"x1":261,"y1":124,"x2":274,"y2":145},"label":{"x1":274,"y1":104,"x2":374,"y2":124}},{"name":"Glen Rock Boro Hall Station","id":"269623093","icon":{"x1":286,"y1":131,"x2":299,"y2":152},"label":{"x1":299,"y1":152,"x2":394,"y2":172}},{"name":"Hawthorne Station","id":"269623100","icon":{"x1":151,"y1":275,"x2":164,"y2":296},"label":{"x1":164,"y1":265,"x2":251,"y2":275}},{"name":"Kingsland Station","id":"269623108","icon":{"x1":356,"y1":1296,"x2":369,"y2":1317},"label":{"x1":369,"y1":1286,"x2":454,"y2":1296}},{"name":"Lyndhurst Station","id":"269623116","icon":{"x1":316,"y1":1248,"x2":329,"y2":1269},"label":{"x1":329,"y1":1238,"x2":413,"y2":1248}},{"name":"New Bridge Landing Station","id":"269623151","icon":{"x1":835,"y1":521,"x2":848,"y2":542},"label":{"x1":848,"y1":501,"x2":945,"y2":521}},{"name":"Oradell Station","id":"269623158","icon":{"x1":864,"y1":189,"x2":877,"y2":210},"label":{"x1":877,"y1":179,"x2":948,"y2":189}},{"name":"Passaic Station","id":"269623162","icon":{"x1":258,"y1":994,"x2":271,"y2":1015},"label":{"x1":271,"y1":984,"x2":346,"y2":994}},{"name":"Paterson Station","id":"269623163","icon":{"x1":64,"y1":488,"x2":77,"y2":509},"label":{"x1":77,"y1":478,"x2":156,"y2":488}},{"name":"Plauderville Station","id":"269623169","icon":{"x1":443,"y1":705,"x2":456,"y2":726},"label":{"x1":456,"y1":695,"x2":550,"y2":705}},{"name":"Radburn Station","id":"269623174","icon":{"x1":331,"y1":299,"x2":344,"y2":320},"label":{"x1":344,"y1":289,"x2":422,"y2":299}},{"name":"River Edge Station","id":"269623181","icon":{"x1":869,"y1":333,"x2":882,"y2":354},"label":{"x1":882,"y1":323,"x2":972,"y2":333}},{"name":"Rutherford Station","id":"269623183","icon":{"x1":453,"y1":1156,"x2":466,"y2":1177},"label":{"x1":466,"y1":1146,"x2":553,"y2":1156}},{"name":"Secaucus Junction Station","id":"269623186","icon":{"x1":597,"y1":1671,"x2":610,"y2":1692},"label":{"x1":610,"y1":1661,"x2":735,"y2":1671}},{"name":"Grove Street Station","id":"269623493","icon":{"x1":790,"y1":1994,"x2":803,"y2":2015},"label":{"x1":694,"y1":1984,"x2":790,"y2":1994}},{"name":"Journal Square Station","id":"269623496","icon":{"x1":673,"y1":1898,"x2":686,"y2":1919},"label":{"x1":686,"y1":1888,"x2":795,"y2":1898}}]}'

		#parse the string
		data = json.loads(data)
		#format the data
		featureCollection = metacutter.extractFeaturesMW(data)
		#didn't lose any in conversion
		self.assertEqual(len(data['pois']), len(featureCollection.features))
		#format the data into sub tiles
		subTiles = metacutter.cutFeatures(featureCollection, (2048, 2048), (8, 8))
		#show the tiles
		for key, value in sorted(subTiles.iteritems()):
			print str(key) + ': ' + str(value) + '\n'

	#test a mapware metadata return
	def test_mapnik_meta(self):
		data = []
		data.append(mnkInmem(Box2d(-235.0,1644.0,-227.0,1657.0), id=u'269623067', name=u'Brick Church Station'))
		data.append(mnkInmem(Box2d(352.766934044,416.233160216,399.766934044,433.233160216), id=u'269623069', name=u'Broadway Station'))
		data.append(mnkInmem(Box2d(372.0,437.0,380.0,450.0), id=u'269623069', name=u'Broadway Station'))
		data.append(mnkInmem(Box2d(136.486298311,834.797056642,171.486298311,851.797056642), id=u'269623073', name=u'Clifton Station'))
		data.append(mnkInmem(Box2d(149.0,856.0,157.0,869.0), id=u'269623073', name=u'Clifton Station'))
		data.append(mnkInmem(Box2d(255.3028736,1117.23930224,309.3028736,1134.23930224), id=u'269623076', name=u'Delawanna Station'))
		data.append(mnkInmem(Box2d(278.0,1138.0,286.0,1151.0), id=u'269623076', name=u'Delawanna Station'))
		data.append(mnkInmem(Box2d(-212.531853511,1656.61436172,-153.531853511,1673.61436172), id=u'269623081', name=u'East Orange Station'))
		data.append(mnkInmem(Box2d(-188.0,1678.0,-180.0,1691.0), id=u'269623081', name=u'East Orange Station'))
		data.append(mnkInmem(Box2d(865.999176533,9.1382502262,907.999176533,26.1382502262), id=u'269623086', name=u'Emerson Station'))
		data.append(mnkInmem(Box2d(882.0,30.0,890.0,43.0), id=u'269623086', name=u'Emerson Station'))
		data.append(mnkInmem(Box2d(714.223903289,750.9391774,774.223903289,767.9391774), id=u'269623087', name=u'Essex Street Station'))
		data.append(mnkInmem(Box2d(740.0,772.0,748.0,785.0), id=u'269623087', name=u'Essex Street Station'))
		data.append(mnkInmem(Box2d(-171.114180267,1354.43933725,-119.114180267,1371.43933725), id=u'269623092', name=u'Glen Ridge Station'))
		data.append(mnkInmem(Box2d(-150.0,1375.0,-142.0,1388.0), id=u'269623092', name=u'Glen Ridge Station'))
		data.append(mnkInmem(Box2d(270.010985956,105.880627681,317.010985956,131.880627681), id=u'269623093', name=u'Glen Rock Boro Hall Station'))
		data.append(mnkInmem(Box2d(289.0,136.0,297.0,149.0), id=u'269623093', name=u'Glen Rock Boro Hall Station'))
		data.append(mnkInmem(Box2d(264.0,129.0,272.0,142.0), id=u'269623094', name=u'Glen Rock Main Line Station'))
		data.append(mnkInmem(Box2d(-180.894971733,790.200402557,-122.894971733,807.200402557), id=u'269623095', name=u'Great Notch Station'))
		data.append(mnkInmem(Box2d(-156.0,811.0,-148.0,824.0), id=u'269623095', name=u'Great Notch Station'))
		data.append(mnkInmem(Box2d(132.292271644,259.232198635,185.292271644,276.232198635), id=u'269623100', name=u'Hawthorne Station'))
		data.append(mnkInmem(Box2d(154.0,280.0,162.0,293.0), id=u'269623100', name=u'Hawthorne Station'))
		data.append(mnkInmem(Box2d(787.105483378,-201.509306658,829.105483378,-184.509306658), id=u'269623104', name=u'Hillsdale Station'))
		data.append(mnkInmem(Box2d(804.0,-181.0,812.0,-168.0), id=u'269623104', name=u'Hillsdale Station'))
		data.append(mnkInmem(Box2d(361.233245156,-162.707178014,410.233245156,-145.707178014), id=u'269623106', name=u'Ho-Ho-Kus Station'))
		data.append(mnkInmem(Box2d(381.0,-142.0,389.0,-129.0), id=u'269623106', name=u'Ho-Ho-Kus Station'))
		data.append(mnkInmem(Box2d(340.609277867,1280.54166677,387.609277867,1297.54166677), id=u'269623108', name=u'Kingsland Station'))
		data.append(mnkInmem(Box2d(360.0,1302.0,368.0,1315.0), id=u'269623108', name=u'Kingsland Station'))
		data.append(mnkInmem(Box2d(300.530372978,1232.40469004,347.530372978,1249.40469004), id=u'269623116', name=u'Lyndhurst Station'))
		data.append(mnkInmem(Box2d(320.0,1253.0,328.0,1266.0), id=u'269623116', name=u'Lyndhurst Station'))
		data.append(mnkInmem(Box2d(596.369878756,1236.20312844,662.369878756,1271.20312844), id=u'269623123', name=u'Meadowlands Sports Complex Station'))
		data.append(mnkInmem(Box2d(625.0,1275.0,633.0,1288.0), id=u'269623123', name=u'Meadowlands Sports Complex Station'))
		data.append(mnkInmem(Box2d(-155.468174933,905.697669212,-110.468174933,931.697669212), id=u'269623132', name=u'Montclair Heights Station'))
		data.append(mnkInmem(Box2d(-137.0,936.0,-129.0,949.0), id=u'269623132', name=u'Montclair Heights Station'))
		data.append(mnkInmem(Box2d(-142.3352384,813.109478146,-65.3352384,839.109478146), id=u'269623133', name=u'Montclair State University Station'))
		data.append(mnkInmem(Box2d(-108.0,843.0,-100.0,856.0), id=u'269623133', name=u'Montclair State University Station'))
		data.append(mnkInmem(Box2d(-171.930818133,973.772392698,-126.930818133,999.772392698), id=u'269623137', name=u'Mountain Avenue Station'))
		data.append(mnkInmem(Box2d(-154.0,1004.0,-146.0,1017.0), id=u'269623137', name=u'Mountain Avenue Station'))
		data.append(mnkInmem(Box2d(-82.2398435555,2084.32078862,-46.2398435555,2110.32078862), id=u'269623148', name=u'Newark Airport Station'))
		data.append(mnkInmem(Box2d(-69.0,2114.0,-61.0,2127.0), id=u'269623148', name=u'Newark Airport Station'))
		data.append(mnkInmem(Box2d(815.033728711,495.909881272,870.033728711,521.909881272), id=u'269623151', name=u'New Bridge Landing Station'))
		data.append(mnkInmem(Box2d(838.0,526.0,846.0,539.0), id=u'269623151', name=u'New Bridge Landing Station'))
		data.append(mnkInmem(Box2d(854.347253333,173.212381955,889.347253333,190.212381955), id=u'269623158', name=u'Oradell Station'))
		data.append(mnkInmem(Box2d(867.0,194.0,875.0,207.0), id=u'269623158', name=u'Oradell Station'))
		data.append(mnkInmem(Box2d(248.481026844,977.920665236,284.481026844,994.920665236), id=u'269623162', name=u'Passaic Station'))
		data.append(mnkInmem(Box2d(262.0,999.0,270.0,1012.0), id=u'269623162', name=u'Passaic Station'))
		data.append(mnkInmem(Box2d(50.4876551111,471.832331594,93.4876551111,488.832331594), id=u'269623163', name=u'Paterson Station'))
		data.append(mnkInmem(Box2d(67.0,493.0,75.0,506.0), id=u'269623163', name=u'Paterson Station'))
		data.append(mnkInmem(Box2d(422.192528356,688.789936899,479.192528356,705.789936899), id=u'269623169', name=u'Plauderville Station'))
		data.append(mnkInmem(Box2d(446.0,710.0,454.0,723.0), id=u'269623169', name=u'Plauderville Station'))
		data.append(mnkInmem(Box2d(319.205597867,282.815117805,359.205597867,299.815117805), id=u'269623174', name=u'Radburn Station'))
		data.append(mnkInmem(Box2d(335.0,304.0,343.0,317.0), id=u'269623174', name=u'Radburn Station'))
		data.append(mnkInmem(Box2d(319.602897778,-37.6913625259,372.602897778,-20.6913625259), id=u'269623180', name=u'Ridgewood Station'))
		data.append(mnkInmem(Box2d(342.0,-17.0,350.0,-4.0), id=u'269623180', name=u'Ridgewood Station'))
		data.append(mnkInmem(Box2d(851.578482489,317.385487941,902.578482489,334.385487941), id=u'269623181', name=u'River Edge Station'))
		data.append(mnkInmem(Box2d(873.0,338.0,881.0,351.0), id=u'269623181', name=u'River Edge Station'))
		data.append(mnkInmem(Box2d(435.765669689,1140.34374683,486.765669689,1157.34374683), id=u'269623183', name=u'Rutherford Station'))
		data.append(mnkInmem(Box2d(457.0,1161.0,465.0,1174.0), id=u'269623183', name=u'Rutherford Station'))
		data.append(mnkInmem(Box2d(582.962345956,1644.62222631,627.962345956,1673.62222631), id=u'269623186', name=u'Secaucus Junction Station'))
		data.append(mnkInmem(Box2d(601.0,1677.0,609.0,1690.0), id=u'269623186', name=u'Secaucus Junction Station'))
		data.append(mnkInmem(Box2d(657.332644978,858.576749935,705.332644978,875.576749935), id=u'269623198', name=u'Teterboro Station'))
		data.append(mnkInmem(Box2d(677.0,880.0,685.0,893.0), id=u'269623198', name=u'Teterboro Station'))
		data.append(mnkInmem(Box2d(-195.483000178,1025.54438345,-150.483000178,1051.54438345), id=u'269623203', name=u'Upper Montclair Station'))
		data.append(mnkInmem(Box2d(-177.0,1056.0,-169.0,1069.0), id=u'269623203', name=u'Upper Montclair Station'))
		data.append(mnkInmem(Box2d(324.0,-256.0,332.0,-243.0), id=u'269623204', name=u'Waldwick Station'))
		data.append(mnkInmem(Box2d(-192.248103111,1216.13063966,-157.248103111,1242.13063966), id=u'269623205', name=u'Walnut Street Station'))
		data.append(mnkInmem(Box2d(-179.0,1246.0,-171.0,1259.0), id=u'269623205', name=u'Walnut Street Station'))
		data.append(mnkInmem(Box2d(-182.902954667,1121.25059759,-134.902954667,1147.25059759), id=u'269623206', name=u'Watchung Avenue Station'))
		data.append(mnkInmem(Box2d(-163.0,1151.0,-155.0,1164.0), id=u'269623206', name=u'Watchung Avenue Station'))
		data.append(mnkInmem(Box2d(-137.183962311,1481.91643675,-82.1839623111,1507.91643675), id=u'269623207', name=u'Watsessing Avenue Station'))
		data.append(mnkInmem(Box2d(-114.0,1512.0,-106.0,1525.0), id=u'269623207', name=u'Watsessing Avenue Station'))
		data.append(mnkInmem(Box2d(831.363281067,-112.477983707,881.363281067,-95.4779837066), id=u'269623210', name=u'Westwood Station'))
		data.append(mnkInmem(Box2d(852.0,-91.0,860.0,-78.0), id=u'269623210', name=u'Westwood Station'))
		data.append(mnkInmem(Box2d(560.604560356,1017.34154093,617.604560356,1034.34154093), id=u'269623214', name=u'Wood Ridge Station'))
		data.append(mnkInmem(Box2d(585.0,1038.0,593.0,1051.0), id=u'269623214', name=u'Wood Ridge Station'))
		data.append(mnkInmem(Box2d(-90.0903552,2233.4763344,-40.0903552,2259.4763344), id=u'269623479', name=u'Newark Airport P1 Station'))
		data.append(mnkInmem(Box2d(-70.0,2263.0,-62.0,2276.0), id=u'269623479', name=u'Newark Airport P1 Station'))
		data.append(mnkInmem(Box2d(-70.2140145778,2197.79144638,-20.2140145778,2223.79144638), id=u'269623481', name=u'Newark Airport P3 Station'))
		data.append(mnkInmem(Box2d(-39.3124999998,2143.125,10.6875000002,2169.125), id=u'269623482', name=u'Newark Airport P4 Station'))
		data.append(mnkInmem(Box2d(-19.0,2173.0,-11.0,2186.0), id=u'269623482', name=u'Newark Airport P4 Station'))
		data.append(mnkInmem(Box2d(-18.0,2247.0,-10.0,2260.0), id=u'269623484', name=u'Newark Terminal A Station'))
		data.append(mnkInmem(Box2d(-4.6249999997,2189.1875,39.3750000003,2215.1875), id=u'269623485', name=u'Newark Terminal B Station'))
		data.append(mnkInmem(Box2d(13.0,2219.0,21.0,2232.0), id=u'269623485', name=u'Newark Terminal B Station'))
		data.append(mnkInmem(Box2d(1046.0,1886.0,1054.0,1899.0), id=u'269623490', name=u'9th St (PATH) Station'))
		data.append(mnkInmem(Box2d(1003.0,1894.0,1011.0,1907.0), id=u'269623491', name=u'Christopher St Station'))
		data.append(mnkInmem(Box2d(780.2828352,1968.8035179,815.2828352,1994.8035179), id=u'269623493', name=u'Grove Street Station'))
		data.append(mnkInmem(Box2d(793.0,1999.0,801.0,2012.0), id=u'269623493', name=u'Grove Street Station'))
		data.append(mnkInmem(Box2d(119.650920533,1825.04950473,160.650920533,1842.04950473), id=u'269623494', name=u'Harrison Station'))
		data.append(mnkInmem(Box2d(136.0,1846.0,144.0,1859.0), id=u'269623494', name=u'Harrison Station'))
		data.append(mnkInmem(Box2d(663.156896,1871.27898766,698.156896,1900.27898766), id=u'269623496', name=u'Journal Square Station'))

		#format the data
		featureCollection = metacutter.extractFeaturesMNK(data)
		count = 0
		for feature in featureCollection.features:
			count += len(feature.geometry.coordinates)
		#didn't lose any in conversion
		self.assertEqual(len(data), count)
		#format the data into sub tiles
		subTiles = metacutter.cutFeatures(featureCollection, (2048, 2048), (8, 8))
		#show the tiles
		for key, value in sorted(subTiles.iteritems()):
			print str(key) + ': ' + str(value) + '\n'

	#dont need this method for now
	#def tearDown:

#so that external modules can run this suite of tests
def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(testMetaCutter))
    return suite

#run the unit tests
#by deriving the testRenderer class from unittest.TestCase
#and then calling unittest.main it looks for any methods named
# test* and runs them
if __name__ == "__main__":
	unittest.TextTestRunner(verbosity=2).run(suite())
