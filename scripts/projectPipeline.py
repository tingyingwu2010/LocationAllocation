#!/usr/bin/python
# -*- coding: utf8 -*-

# Script to start ONE simulations

from multiprocessing.dummy import Pool
from subprocess import Popen, PIPE, STDOUT
import optparse
import re
import time
import json
import urllib2
import os
import copy

NB_PROCESSES = 7
regex = "(?:[\d\.\w\-]+\,)+[\d\.\w\-]+"
URL_BASE = "http://localhost:8080"
NB_MAX_REQ = 10


def send_request(base, params={}):
    """ Forms the request to send to the API and returns the data if no errors occurred
    :param base: base URL to send the request
    :param params: url attributes (key=value)
    :rtype: data dictionary
    """
    nb_req = 0
    while nb_req < NB_MAX_REQ:
        full_url = base + "?" + "&".join(["%s=%s" % (k, str(v).replace(' ', '+')) for (k, v) in params.items()])
        print full_url

        try:
            data = json.load(urllib2.urlopen(full_url))
        except:
            print "failed", nb_req
            nb_req += 1
            time.sleep(min(1, nb_req / 10))
            continue

        return data
    return None


def get_allocation_file(out, method, nbFacilities, deadline, delFactor, travelTime, distance):
    """
    Sends a request to the allocation server and get the response from the server
    :param out: output file
    :param method: allocation method to allocate the facilities (loc|pgrk|kmeans)
    :param nbFacilities: number of facilities to allocate
    :param deadline: deadline of the requests
    :param delFactor: deletion factor to delete candidates around allocated facilities
    :param travelTime: travel time bound between two candidate locations
    :param distance: distance bound between two candidaate locations
    :rtype: number of facilities allocated
    """
    data = send_request(URL_BASE + "/allocation/" + method, {"nbFacilities": nbFacilities,
                                                             "deadline": deadline,
                                                             "delFactor": delFactor,
                                                             "travelTime": travelTime,
                                                             "distance": distance})
    results = data.get("allocationResult")
    count = 0
    with open(out, 'w') as f:
        for facility_id, facility in results.items():
            print facility
            f.write("{id} {x} {y} {rank} {weight} {nbDeleted} {nbAllocated}\n".format(id=facility_id,
                                                                                 x=facility["x"],
                                                                                 y=facility["y"],
                                                                                 rank=facility["rank"],
                                                                                 weight=facility["weight"],
                                                                                 nbDeleted=facility["nbDeleted"],
                                                                                 nbAllocated=facility["nbAllocated"]))

            count += 1

    return count


def run(settings, key, values, key2="", values2=[]):
    """
    Run one instance of the simulation
    :param settings:
    :param key:
    :param values:
    :param key2:
    :param values2:
    :rtype allocation result dictionary
    """
    processes = []
    file_count = 1
    for v in values:
        settings[key] = v
        s1 = copy.deepcopy(settings)
        if key2:
            for v2 in values2:
                s2 = copy.deepcopy(s1)
                s2[key2] = v2
                processes.append((s2, v, v2, file_count))
                file_count += 1
        else:
            processes.append((s1, v, "", file_count))
            file_count += 1

    def get_lines((s, v1, v2, count)):
        """
        Runs the location allocation procedure then run the simulations
        :return: the output of the simulations
        """
        print_str = "Execute for %s = %s" % (key, v1)
        if key2:
            print_str += " and %s = %s" % (key2, v2)
        print print_str

        if s["facilityPlacement"] == "file":
            if(s["allocPath"]):
                deadline = s['deadline']
                if s['deadline'] > 2400:
                    deadline = 2400
                alloc_file = s["allocPath"] + "alloc"+str(s['nrofFacilities'])+"-"+str(deadline)+".txt" #+str(s['deadline'])+".txt"
                s["facilitiesFilename"] = alloc_file
                print alloc_file, os.path.isfile(alloc_file)
                if(not os.path.isfile(alloc_file)):
                    return
            else:
                out = os.getcwd() + "/alloc-" + str(count) + ".txt"
                s["facilitiesFilename"] = out
                nbFacilities = get_allocation_file(out,
                                    s['method'],
                                    s['nrofFacilities'],
                                    s['deadline'],
                                    s['delFactor'],
                                    s['travelTime'],
                                    s['distance'])
                s["nrofFacilities"] = nbFacilities
        f = build_settings_file(s)
        print f
        process = Popen("echo \"{str}\" | java -jar ONE.jar -b {b} /dev/stdin".format(str=f, b=s["b"]),
                        shell=True, stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True)

        return process.communicate()[0]  # get the output, [1] is the error

    outputs = Pool(NB_PROCESSES).map(get_lines, processes)

    # remove the file we just created
    if(not settings["allocPath"]):
        for i in range(1,file_count):
            filepath = os.getcwd() + "/alloc-" + str(i) + ".txt"
            os.remove(filepath)

    # process the output of the simulations
    res = []
    for out in outputs:
        print out
        if out :
            m = re.findall(regex, out)
            res += m

    return res


def get_options():
    optParser = optparse.OptionParser()
    optParser.add_option("-f", "--file", action="store", dest='file', help="setting file")
    optParser.add_option("-m", "--mobility", action="store", dest='mobility', help='type of mobility', default="")
    optParser.add_option("-a", "--mobility-file", action="store", dest='mobilityFile', help='mobility file', default="")
    optParser.add_option("-d", "--deadline", action="store", dest='deadline', help='deadline of the requests',
                         default=120, type="int")
    optParser.add_option("-s", "--max-storage", action="store", dest='maxStorage',
                         help='maximum storage of the storage facilities', default=-1, type="int")
    optParser.add_option("-n", "--nb-facilities", action="store", dest='nrofFacilities',
                         help='number of storage facilities', default=-1, type="int")
    optParser.add_option("-p", "--placement", action="store", dest='facilityPlacement',
                         help='placement of the storage facilities', default="random")
    optParser.add_option("-r", "--rep", action="store", dest='replicationType', help='type of file replication',
                         default="rep")
    optParser.add_option("-u", "--users", action="store", dest='nrofMobileUsers', help='number of mobile users',
                         default=100)
    options, args = optParser.parse_args()
    return options


def build_settings_file(settings):
    # get the arguments and construct the variables
    if settings["file"]:
        with open(settings["file"], "r") as f:
            return f.read()

    nrofHostGroups = 1
    movementModel = ""
    mobility = settings["mobility"]
    mobilityFile = settings["mobilityFile"]
    simTime = settings["simTime"]
    nrofMobileUsers = settings["nrofMobileUsers"]

    maxStorage = ""
    if settings["maxStorage"] not in [-1, ""]:
        maxStorage = "FileSystemReport2.maxStorage = %s" % (settings["maxStorage"])

    if settings["deadline"] != -1:
        deadline = settings["deadline"]
    else:
        deadline = "240"

    if mobility == "WorkingDayMovement":
        nrofHostGroups = 3
        # Define another group of nodes
        movementModel = """
MovementModel.worldSize = 5000, 5000

Group2.groupID = A
Group2.nrofHosts = 100
Group2.busControlSystemNr = 1
Group2.shoppingControlSystemNr = 1
Group2.nrOfOffices = 1
Group2.nrOfMeetingSpots = 1
Group2.movementModel = {mobility}
Group2.officeLocationsFile = filesystem/WDM-g1-offices.wkt
Group2.homeLocationsFile   = filesystem/WDM-g1-homes.wkt
Group2.meetingSpotsFile    = filesystem/WDM-g1-offices.wkt

Group3.groupID = B
Group3.nrofHosts = 25
Group3.busControlSystemNr = 2
Group3.shoppingControlSystemNr = 2
Group3.nrOfOffices = 3
Group3.nrOfMeetingSpots = 0
Group3.movementModel = {mobility}
Group3.officeLocationsFile = filesystem/WDM-g2-offices.wkt
Group3.homeLocationsFile   = filesystem/WDM-g2-homes.wkt
Group3.meetingSpotsFile    = filesystem/WDM-g2-offices.wkt
""".format(mobility=mobility)

    elif mobility == "ManhattanGridMovement":
        nrofHostGroups = 2
        movementModel = """
MovementModel.worldSize = 5000, 5000
Group2.groupID = t
Group2.nrofHosts = {nrofMobileUsers}
Group2.movementModel = {mobility}
ManhattanGridMovement.nbLines = 5
ManhattanGridMovement.nbColumns = 3
""".format(mobility=mobility, nrofMobileUsers=nrofMobileUsers)

    elif mobility == "RandomWaypoint":
        nrofHostGroups = 2
        movementModel = """
MovementModel.worldSize = 5000, 5000
Group2.groupID = t
Group2.nrofHosts = {nrofMobileUsers}
Group2.movementModel = RandomWaypoint
""".format(mobility=mobility, nrofMobileUsers=nrofMobileUsers)

    elif mobility == "MapBasedMovement":
        nrofHostGroups = 2
        movementModel = """
MovementModel.worldSize = 5000, 5000
Group2.groupID = t
Group2.nrofHosts = {nrofMobileUsers}
Group2.movementModel = MapBasedMovement
MapBasedMovement.nrofMapFiles = 1
MapBasedMovement.mapFile1 = /Users/ben/Documents/workspace/ONE/src/one_1.4.1/data/FileSystem/world.wkt
""".format(mobility=mobility, nrofMobileUsers=nrofMobileUsers)
    elif mobility == "" and mobilityFile != "":
        print "hello", mobilityFile
        with open(mobilityFile, "r") as file_mobility:
            nrofHostGroups = 1+int(next(file_mobility))
            for line in file_mobility:
                movementModel += line

    if settings["facilityPlacement"] in ["random", "grid", "file"]:
        facilityPlacement = settings["facilityPlacement"]
    if settings["facilityPlacement"] == "grid" and mobility == "ManhattanGridMovement":
        facilityPlacement = "manhattanGrid"
    facilitiesFilename = ""
    if settings["facilityPlacement"] == "file" and settings["facilitiesFilename"]:
        facilitiesFilename = "FileSystemBackendReport.facilitiesFilename = %s" % settings["facilitiesFilename"]

    propagationTimer = settings["propagationTimer"]
    nrofFileCopies = settings["nrofFileCopies"]
    useBackendNodes = "true" if settings["useBackendNodes"] else "false"
    useGlobalInformation = "true" if settings["useGlobalInformation"] else "false"
    putPolicy = settings["putPolicy"]
    outputAvailabilityTimes = "true" if settings["outputAvailabilityTimes"] else "false"

    if settings["nrofFacilities"] != -1:
        nrofFacilities = settings["nrofFacilities"]
    else:
        nrofFacilities = "[1; 2; 3]"  # "; 4; 5; 10; 15; 20; 25; 40; 45; 50; 75; 100; 125; 150]"

    str = """
Scenario.name = filesystem_scenario
Scenario.simulateConnections = true
Scenario.updateInterval = 0.1
Scenario.endTime = {simTime}
MovementModel.rngSeed = 42

wifi.type = SimpleBroadcastInterface
wifi.transmitSpeed = 10M
wifi.transmitRange = 150

Events.nrof = 0
Scenario.nrofHostGroups = {nrofHostGroups}

Group.router = DirectDeliveryRouter
Group.nrofInterfaces = 1
Group.interface1 = wifi
Group.speed = 2.7, 13.9

{movementModel}

Group{nrofHostGroups}.groupID = f
Group{nrofHostGroups}.nrofHosts = {nrofFacilities}
Group{nrofHostGroups}.movementModel = StationaryMovement
# Group{nrofHostGroups}.movementModel = RandomWaypoint # To make the facilities move
Group{nrofHostGroups}.nodeLocation = -1, -1

Report.nrofReports = 1
Report.warmup = 0
Report.reportDir = reports/

Report.report1 = FileSystemBackendReport
FileSystemBackendReport.reqGenRate = 0.0016667
FileSystemBackendReport.proportionPutReq = 0.1
FileSystemBackendReport.deadline = {deadline}
FileSystemBackendReport.putPolicy = {putPolicy}
FileSystemBackendReport.useBackendNodes = {useBackendNodes}
FileSystemBackendReport.propagationTimer = {propagationTimer}
FileSystemBackendReport.nrofFileCopies = {nrofFileCopies}
{facilitiesFilename}
FileSystemBackendReport.outputAvailabilityTimes = {outputAvailabilityTimes}
""".format(simTime=settings["simTime"], nrofHostGroups=nrofHostGroups, nrofFacilities=nrofFacilities,
           movementModel=movementModel, deadline=deadline, facilityPlacement=facilityPlacement, maxStorage=maxStorage,
           facilitiesFilename=facilitiesFilename, propagationTimer=propagationTimer, nrofFileCopies=nrofFileCopies,
           useBackendNodes=useBackendNodes,useGlobalInformation=useGlobalInformation,putPolicy=putPolicy,outputAvailabilityTimes=outputAvailabilityTimes)
    return str


def run_allocation_thread(n):
    out = "alloc-"+str(n)+".txt"
    get_allocation_file(out, "loc", n, 500, -1, -1, -1)
    return out


if __name__ == '__main__':
    ## test the allocation function
    # get_allocation_file("alloc0.txt", "loc", 10, 500, -1, -1, -1)

    ## test the allocation function in parallel
    # p = Pool(5)
    # print p.map(run_allocation_thread, [10,20,30,40])

    options = get_options()
    settings = {
        "file": options.file,
        "simTime": 21400,
        "deadline": 20000,  # options.deadline, 400, 500, 600, 700, 800, 900, 1000, 1500, 2000, 2500, 3000, 3600
        "mobility": "",    # "ManhattanGridMovement", # "RandomWaypoint", "MapBasedMovement", "" # options.mobility,
        "mobilityFile": "settings.txt", 
        "nrofFacilities": 15,  # options.nrofFacilities,
        "nrofMobileUsers": 50,
        "b": 1,  # number of simulations in batch to launch
        "maxStorage": -1,
        "facilityPlacement": "file",
        "facilitiesFilename": "",
        "method": "loc",
        "delFactor": 0.5,
        "travelTime": "1000",
        "distance": "1000",
        "propagationTimer": -1,
        "nrofFileCopies": -1,
        "useGlobalInformation": True,
        "useBackendNodes": True,
        "putPolicy": "first",  # {"all", "first"}
        "allocPath": "../data/sf-muni-ONE/alloc/",
        "outputAvailabilityTimes": True # "../data/TestSynthetic/alloc/"
    }

    count = 0
    # out = os.getcwd() + "/alloc-" + str(count) + ".txt"
    # settings["facilitiesFilename"] = out
    # nbFacilities = get_allocation_file(out,
    #                     settings['method'],
    #                     settings['nrofFacilities'],
    #                     settings['deadline'],
    #                     settings['delFactor'],
    #                     settings['travelTime'],
    #                     settings['distance'])

    out = run(settings, "nrofFacilities", [19,20,21,22])
    for o in out:
        print o
