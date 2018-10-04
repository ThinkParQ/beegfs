BeeGFS monitoring service README
================================


Introduction
------------

The BeeGFS monitoring service (beegfs-mon) collects statistical data from the
various BeeGFS nodes and stores it into a time series database (at the moment InfluxDB and Apache
Cassandra are supported).


Prerequisites and dependencies
------------------------------

We highly recommend to use InfluxDB as backend unless you already have a Cassandra Cluster in use
that you want to utilize for mon. The next sections only refer to InfluxDB, if you want to use
Cassandra, please refer to the last paragraph.

InfluxDB and Grafana are NOT included within this package for several reasons:

* The user might want to run the InfluxDB server on another machine and/or wants
  to integrate the beegfs-mon into an already existing setup.
* The user might want to use his own or other thirdparty tools to evaluate the
  collected data
* They can be updated independently by the user whenever he wants to.

So, to use beegfs-mon, a working and reachable InfluxDB setup is required. Installing
InfluxDB should be simple in most cases since there are prebuilt packages available
for all of the distributions that are supported by BeeGFS.
The installation instructions can be found at

https://docs.influxdata.com/influxdb/v1.3/introduction/installation/ .


Grafana, on the other hand, is completely optional. It's completely up to the user what
he wants to do with the data stored in the database. However, for the sake of simplicity,
we provide some prebuilt Grafana dashboards that can be easily imported into the
Grafana setup and used for monitoring. The installation instructions can be
found at

http://docs.grafana.org/installation/ .



Installation
------------

### Meet the prerequisites

If there isn't an already running InfluxDB service that you want to use, install and start
InfluxDB first (look above for the link to the installation documentation).
If the service runs on another host, make sure it is reachable via HTTP.

### Grafana Dashboards

If you want an out of the box solution, you should use the provided Grafana panels
for visualization. So, install Grafana (again, look above for
the installation instructions) and make sure it can reach the InfluxDB service via network.


#### Default installation

You can then use the provided installation script which can be found under
scripts/import-dashboards. For the out-of-the-box setup with InfluxDB and Grafana being
on the same host, just use

import-dashboards default


#### Custom installation

In any other case, either provide the script with the URLs to InfluxDB and Grafana
(call the script without arguments for usage instruction) or install them manually.
The latter can be done from within Grafanas web interface:

First, the datasource must be defined. In the main menu, click on "Data Sources" and
then "Add Data Source". Enter a name, hostname and port where your InfluxDB is running. Save.

To add the dashboards, select "Dashboards/Import" in the main menu. Navigate to [...] and select
one of the dashboard .json files. Select the datasource you created before in the dropdown and
click "Import". Repeat for the rest of the panels.

You can now click on "Dashboards" in the main menu and then on the Button to the right of it.
A list of the installed dashboards should pop up, in which you can select the one you want to watch.
If your BeeGFS setup, the beegfs-mon daemon and InfluxDB are already running and are configured
properly, you should already see some data being collected.


For more documentation and help in using Grafana, please visit the official website
http://docs.grafana.org.


Apache Cassandra
----------------

If you want to use Cassandra, please be aware that currently there are no Grafana panels for it
available.

To configure beegfs-mon to use Cassandra, you need to install the datastax cassandra client library
on your system which you can find here: https://github.com/datastax/cpp-driver.
It has to be the version 2.9. beegfs-mon loads the library dynamically, so no recompilation is
required. The beegfs-mon config file needs to be edited to use the cassandra plugin. The available 
options are explained over there.
