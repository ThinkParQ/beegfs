package com.beegfs.admon.gui.common;


import com.beegfs.admon.gui.common.enums.MetaOpsEnum;
import com.beegfs.admon.gui.common.enums.StatsTypeEnum;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_CLIENT_METADATA;
import static com.beegfs.admon.gui.common.enums.StatsTypeEnum.STATS_USER_METADATA;
import com.beegfs.admon.gui.common.enums.StorageOpsEnum;
import com.beegfs.admon.gui.common.enums.quota.GetQuotaEnum;
import com.beegfs.admon.gui.common.enums.quota.SetQuotaIDListEnum;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.GuiThread;
import com.beegfs.admon.gui.common.tools.DefinesTk;
import com.beegfs.admon.gui.components.charts.ChartPoint;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.StringReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.TreeMap;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.jdom.Attribute;
import org.jdom.Document;
import org.jdom.Element;
import org.jdom.JDOMException;
import org.jdom.filter.ElementFilter;
import org.jdom.input.SAXBuilder;
import org.xml.sax.InputSource;

/**
 * Requests the data from the admon daemon and parses the XML data.
 */
public class XMLParser extends GuiThread
{
   static final Logger LOGGER = Logger.getLogger(XMLParser.class.getCanonicalName());

   private final static int INITIAL_CAPACITY = 10;
   private static final boolean DEBUG_POST = false;
   
   private Element rootElement;
   private String url;
   private boolean commError;
   private String errorMsg;
   private boolean oneShot;
   final Lock lock;
   final Condition gotData;
   final int loopSleepMS;

   /**
    * Requests the data from the admon daemon.
    *
    * @param url The URL of the XML to retrieve
    * @param oneShot If true request the data only once.
    * @param name The thread name
    */
   public XMLParser(String url, boolean oneShot, String name)
   {
      this(url, oneShot, new ReentrantLock(), DEFAULT_LOOP_SLEEP_MS, name);
   }

   /**
    * Requests the data from the admon daemon.
    *
    * @param url The URL of the XML to retrieve
    * @param name The thread name
    */
   public XMLParser(String url, String name)
   {
      this(url, false, name);
   }

   /**
    * Requests the data from the admon daemon.
    *
    * @param url The URL of the XML to retrieve
    * @param lock The lock for the data variable
    * @param name The thread name
    */
   public XMLParser(String url, Lock lock, String name)
   {
      this(url, false, lock, DEFAULT_LOOP_SLEEP_MS, name);
   }

   /**
    * Requests the data from the admon daemon.
    *
    * @param url The URL of the XML to retrieve
    * @param oneShot If true request the data only once.
    * @param lock The lock for the data variable
    * @param name The thread name
    */
   public XMLParser(String url, boolean oneShot, Lock lock, String name)
   {
      this(url, oneShot, lock, DEFAULT_LOOP_SLEEP_MS, name);
   }

   /**
    * Requests the data from the admon daemon.
    *
    * @param url The URL of the XML to retrieve
    * @param oneShot If true request the data only once.
    * @param lock The lock for the data variable
    * @param loopSleepMS The request interval
    * @param name The thread name
    */
   public XMLParser(String url, boolean oneShot, Lock lock, int loopSleepMS, String name)
   {
      super(name);
      this.lock = lock;
      this.gotData = lock.newCondition();
      this.url = url;
      this.rootElement = new Element("data");
      this.commError = false;
      this.oneShot = oneShot;
      this.loopSleepMS = loopSleepMS;
   }

   /**
    * Requests the data from the admon daemon.
    *
    * @param name The thread name
    */
   public XMLParser(String name)
   {
      this("", name);
   }

   /**
    * Getter for the condition variable which notifies when new data arrives
    *
    * @return The condition varaible
    */
   public Condition getDataCondition()
   {
      return this.gotData;
   }

   /**
    * Changes the request URL
    *
    * @param url The URL of the XML to retrieve
    */
   public void setUrl(String url)
   {
      this.url = url;
   }

   /**
    * Add a new parameter to the request URL
    *
    * @param param The parameter name
    * @param value The parameter value
    * @return The new URL string
    */
   public String addParam(String param, String value)
   {
      if (url.contains("?"))
      {
         return url.concat("&" + param + "=" + value);
      }
      else
      {
         return url.concat("?" + param + "=" + value);
      }
   }

   @Override
   public void shouldStop()
   {
      super.shouldStop();

      lock.lock();
      try
      {
         gotData.signalAll();
      }
      finally
      {
         lock.unlock();
      }
   }

   /**
    * The run methode of the update thread
    */
   @Override
   public void run()
   {
      if (oneShot)
      {
         update();
      }
      else
      {
         while (!stop)
         {
            update();
            try
            {
               sleep(loopSleepMS);
            }
            catch (InterruptedException e)
            {
               LOGGER.log(Level.SEVERE, "Interrupted Exception in XML Parser run.", e);
            }
         }
      }
   }

   /**
    * Fetch the information from the URL saved in XMLParser object and save it for further use
    * 
    * @return false if a error occurs during requesting the data from the admon daemon.
    */
   public boolean update()
   {
      boolean retVal = false;
      String urlForLog = url;
      int rootElementSizeForLog = 0;

      try
      {
         URL urlObject;
         if(url != null)
         {
            urlObject = new URL(url);
            LOGGER.log(Level.FINEST, "request URL: {0}", url);
         }
         else
         {
            return false;
         }

         SAXBuilder parser = new SAXBuilder(false);
         Document doc;

         if(DEBUG_POST)
         {
            HttpURLConnection con = (HttpURLConnection) urlObject.openConnection();
            con.setRequestMethod("GET");
            int responseCode = con.getResponseCode();

            StringBuilder response;
            try (BufferedReader in = new BufferedReader(new InputStreamReader(con.getInputStream(),
               DefinesTk.DEFAULT_ENCODING_UTF8) ) )
            {
               response = new StringBuilder(DefinesTk.DEFAULT_STRING_LENGTH);
               String inputLine = in.readLine();

               while (inputLine != null)
               {
                  response.append(inputLine);
                  inputLine = in.readLine();
               }
            }

            LOGGER.log(Level.SEVERE, "Sending \'GET\' request to URL: {0}; Response Code: {1}; " +
               "Response: {2}", new Object[]{url, responseCode, response.toString()});

            doc = parser.build(new InputSource(new StringReader(response.toString() ) ) );
         }
         else
         {
            doc = parser.build(urlObject);
         }

         synchronized (this)
         {
            rootElement = doc.getRootElement();
            rootElementSizeForLog = rootElement.getChildren().size();
            retVal = true;
         }
         lock.lock();
         try
         {
            gotData.signal();
         }
         finally
         {
            lock.unlock();
         }
         commError = false;

         return retVal;
      }
      catch (IOException e)
      {
         commError = true;
         errorMsg = e.getMessage();
         LOGGER.log(Level.SEVERE, "Communication error during requesting URL: " + urlForLog +
            " during XML Parser update.", e);
         retVal = false;
      }
      catch (JDOMException e)
      {
         commError = true;
         errorMsg = e.getMessage();
         LOGGER.log(Level.SEVERE, "Exception in XML Parser update.", e);
         retVal = false;
      }
      finally
      {
         if (!commError)
         {
            LOGGER.log(Level.FINEST, "data recieved from URL: {0} ; result size: {1}",
               new Object[]{urlForLog, rootElementSizeForLog});
         }
      }
      return retVal;
   }

   /**
    * Fetch information from an InputStream returned by server
    *
    * The internal data of the parser is updated, but the URL is not. A call to update will
    * therefore retrieve information about the URL provided and overwrite the information read here.
    * The stream will not be closed!
    *
    * @param inStream The stream to read from
    */
   public void readFromInputStream(InputStream inStream)
   {
      try
      {
         SAXBuilder parser = new SAXBuilder();
         Document doc = parser.build(inStream);
         synchronized (this)
         {
            rootElement = doc.getRootElement();
         }
         commError = false;
      }
      catch (JDOMException | IOException e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser readFromInputStream.", e);
      }
   }

   /**
    * Get the information saved as Hashmap with elements in the form (tagname,value)
    *
    * @return TreeMap<String,String> the keys and values from the childs of the root element
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public TreeMap<String, String> getTreeMap() throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      TreeMap<String, String> map = new TreeMap<>();

      try
      {
         synchronized (this)
         {
            @SuppressWarnings("unchecked")
            List<Element> nodes = rootElement.getChildren();
            for (Element node : nodes)
            {
               map.put(node.getName(), node.getValue());
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getTreeMap", e);
      }

      return map;
   }

   /**
    * Get the information saved as Hashmap with elements in the form (tagname,value), but only for
    * elements enclosed in the parent tag
    *
    * @param parent the parent tag
    * @return TreeMap<String,String> the keys and values from the childs of the given parent
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public TreeMap<String, String> getTreeMap(String parent) throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      TreeMap<String, String> map = new TreeMap<>();

      try
      {
         synchronized (this)
         {
            Element child = rootElement.getChild(parent);
            if (child != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> nodes = child.getChildren();
               for (Element node : nodes)
               {
                  map.put(node.getName(), node.getValue());
               }
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getTreeMap", e);
      }

      return map;
   }

   /**
    * Get all elements enumerated in an XML file (enclosed in the parent tag) as vector
    *
    * @param parent the parent tag
    * @return ArrayList<String> the list of all child elements from the given parent
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public ArrayList<String> getVector(String parent) throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      ArrayList<String> vec = null;

      try
      {
         synchronized (this)
         {
            Element child = rootElement.getChild(parent);
            if (child != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> nodes = child.getChildren();
               vec = new ArrayList<>(nodes.size());

               for (Element node : nodes)
               {
                  vec.add(node.getValue());
               }
            }
         }
      }
      catch (java.lang.NullPointerException e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getVector.", e);
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getVector.", e);
      }

      if (vec == null)
      {
         vec = new ArrayList<>(0);
      }

      return vec;
   }

   /**
    * Get all elements enumerated in an XML file (enclosed in the root tag) as vector
    *
    * @return ArrayList<String> the list of all child elements from the root element
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public ArrayList<String> getVector() throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      ArrayList<String> vec = null;

      try
      {
         synchronized (this)
         {
            @SuppressWarnings("unchecked")
            List<Element> nodes = rootElement.getChildren();
            vec = new ArrayList<>(nodes.size());
            for (Element node : nodes)
            {
               vec.add(node.getValue());
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getVector", e);
      }

      if(vec == null)
      {
         vec = new ArrayList<>(0);
      }

      return vec;
   }

   /**
    * Get enumerated values as TreeMap in the form of (element attr, element value)
    *
    * Example : <value node="foo">true</value> <value node="bar">false</value>
    *
    * would return (with attr="node") the map [ (foo,true),(bar,false) ]
    *
    * @param attr the attribute to retrieve
    * @return TreeMap<String,String> the keys and values of all childs from the root element with
    *    the given attribute
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public TreeMap<String, String> getAttrValueMap(String attr) throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      TreeMap<String, String> map = new TreeMap<>();

      try
      {
         synchronized (this)
         {
            @SuppressWarnings("unchecked")
            List<Element> nodes = rootElement.getChildren();
            for (Element node : nodes)
            {
               map.put(node.getAttributeValue(attr), node.getValue());
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getAttrValueMap", e);
      }

      return map;
   }

   /**
    * Get enumerated values as TreeMap in the form of (element attr, element value), but only
    * elements inside parent
    *
    * Example : <nodes> <value node="foo">true</value> <value node="bar">false</value> </nodes>
    * would return (with attr="node" and parent="nodes") the map [ (foo,true),(bar,false) ]
    *
    * @param parent the enclosing element
    * @param attr the attribute to retrieve
    * @return TreeMap<String,String> the keys and values of all childs from the parent element with
    *    the given attribute
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public TreeMap<String, String> getAttrValueMap(String parent, String attr)
           throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      TreeMap<String, String> map = new TreeMap<>();
      try
      {
         synchronized (this)
         {
            Element child = rootElement.getChild(parent);
            if (child != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> nodes = child.getChildren();
               for (Element node : nodes)
               {
                  map.put(node.getAttributeValue(attr), node.getValue());
               }
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getAttrValueMap", e);
      }

      return map;
   }

   /**
    * Get chart points as ArrayList in the form of (time,value), but only elements inside parent
    *
    * @param parent the enclosing element
    * @return ArrayList<ChartPoint> with all chart points from the given parent
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public ArrayList<ChartPoint> getChartPointArrayList(String parent) throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      ArrayList<ChartPoint> list = null;

      try
      {
         synchronized (this)
         {
            Element child = rootElement.getChild(parent);
            if (child != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> nodes = child.getChildren();
               list = new ArrayList<>(nodes.size());
               for (Element node : nodes)
               {
                  // getting it in seconds, directly making millisecs
                  double time = Double.parseDouble(node.getAttributeValue("time"));
                  double value = Double.parseDouble(node.getValue());
                  list.add(new ChartPoint(time, value));
               }
            }
         }
      }
      catch (NumberFormatException e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getAttrValueMap", e);
      }

      if (list == null)
      {
         list = new ArrayList<>(0);
      }

      return list;
   }

   /**
    * Get stats values of the given stat type
    *
    * @param type the type of the stats to request StatsTypeEnum
    * @return Object[][] with the stats of the given stat type
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public Object[][] getStats(StatsTypeEnum type) throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      Object[][] list = null;

      int columnCount;
      if (type == STATS_CLIENT_METADATA || type == STATS_USER_METADATA)
      {
         columnCount = MetaOpsEnum.values().length;
      }
      else
      {
         columnCount = StorageOpsEnum.values().length;
      }

      try
      {
         synchronized (this)
         {
            Element sumChild = rootElement.getChild("sum");
            Element hostsChild = rootElement.getChild("hosts");

            int rowIndex = 0;
            
            if (sumChild != null && hostsChild != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> sum = sumChild.getChildren();
               
               @SuppressWarnings("unchecked")
               List<Element> hosts = hostsChild.getChildren();

               list = new Object[1 + hosts.size()][columnCount];

               if ((sum.size() + hosts.size()) == 0)
               {
                  list[0][0] = "no I/O happened";
               }

               // the sum of all client stats
               list[rowIndex][0] = "sum";
               for (Element stat : sum)
               {
                  int statID = Integer.parseInt(stat.getAttributeValue("id")) + 1;
                  String statValue = stat.getValue();
                  list[rowIndex][statID] = statValue;
               }

               boolean unkownStatIDLogged = false;
               rowIndex++;
               for (Element host : hosts)
               {
                  String ip = host.getAttributeValue("ip");
                  list[rowIndex][0] = ip;

                  @SuppressWarnings("unchecked")
                  List<Element>  stats = host.getChildren();
                  for (Element stat : stats)
                  {
                     int statID = Integer.parseInt(stat.getAttributeValue("id")) + 1;
                     String statValue = stat.getValue();

                     if(statID < columnCount)
                     {
                        list[rowIndex][statID] = statValue;
                     }
                     else
                     {
                        if(!unkownStatIDLogged)
                        {
                           LOGGER.log(Level.FINEST, "Unknown statID: {0}, New stat added? ",
                              String.valueOf(statID) );
                        }
                     }
                  }
                  rowIndex++;
               }
            }
            else
            {
               list = new Object[1][columnCount];
               list[0][0] = "no I/O happened";
            }
         }
      }
      catch (NumberFormatException e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getAttrValueMap", e);
      }

      return list;
   }

   /**
    * Get quota data
    *
    * @return Object[][] with the quota data
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public Object[][] getQuotaData() throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      Object[][] list = null;
      int columnCount = GetQuotaEnum.getColumnCount();

      try
      {
         synchronized (this)
         {
            Element quotasChild = rootElement.getChild("quotas");

            int rowIndex = 0;

            if (quotasChild != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> quotas = quotasChild.getChildren();

               list = new Object[quotas.size()][columnCount];

               // the sum of all client stats
               for (Element quota : quotas)
               {
                  GetQuotaEnum[] enumValues = GetQuotaEnum.values();
                  for (GetQuotaEnum enumValue : enumValues)
                  {
                     String value = "";
                     int valueIndex = enumValue.getColumn();
                     Element element = quota.getChild(enumValue.getAttributeValue());
                     if (element != null)
                     {
                        value = element.getValue();
                     }
                     list[rowIndex][valueIndex] = value;
                  }

                  rowIndex++;
               }
            }
            else
            {
               list = new Object[1][columnCount];
               list[0][0] = "no quota data available";
            }
         }
      }
      catch (NumberFormatException e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getAttrValueMap", e);
      }

      return list;
   }

   /**
    * Get quota limits
    *
    * @return Object[][] with the quota limits
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public ArrayList<ArrayList<Object>> getQuotaLimits() throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      ArrayList<ArrayList<Object>> list = null;
      int columnCount = SetQuotaIDListEnum.getColumnCount();

      try
      {
         synchronized (this)
         {
            Element quotasChild = rootElement.getChild("quotas");

            int rowIndex = 0;

            if (quotasChild != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> quotas = quotasChild.getChildren();

               list = new ArrayList<>(quotas.size());

               // the sum of all client stats
               for (Element quota : quotas)
               {
                  SetQuotaIDListEnum[] enumValues = SetQuotaIDListEnum.values();
                  for (SetQuotaIDListEnum enumValue : enumValues)
                  {
                     String value = "";
                     int columnIndex = enumValue.getColumn();
                     Element element = quota.getChild(enumValue.getAttributeValue());
                     if (element != null)
                     {
                        value = element.getValue();
                     }
                     list.get(rowIndex).set(columnIndex, value);
                  }

                  rowIndex++;
               }
            }
            else
            {
               list = new ArrayList<>(1);
               list.add(new ArrayList<>(columnCount));
               list.get(0).add("no quota data available");
            }
         }
      }
      catch (NumberFormatException e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getAttrValueMap", e);
      }

      return list;
   }

   /**
    * Get the value of exactly one element which is underneath the top level element
    *
    * @param element the name of the element
    * @return String the value of the element
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public String getValue(String element) throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      String outStr = "";

      try
      {
         synchronized (this)
         {
            Element elem = rootElement.getChild(element);
            if (elem != null)
            {
               outStr = elem.getValue();
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getValue", e);
      }

      return outStr;
   }

   /**
    * Get the value of exactly one element which is underneath a parent element
    *
    * @param parent the enclosing element
    * @param element the name of the element
    * @return String the value of the element
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public String getValue(String element, String parent) throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      String outStr = "";

      try
      {
         synchronized (this)
         {
            Element parentElement = rootElement.getChild(parent);
            if (parentElement!= null)
            {
               Element elem = parentElement.getChild(element);
               if (elem != null)
               {
                  outStr = elem.getValue();
               }
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getValue", e);
      }

      return outStr;
   }

   /**
    * Get a vector of enumerated values as TreeMap in the form of (element attr, element value),
    * each vector element matches the enclosing tag
    *
    * Example : <nodes> <value node="foo">true</value> <value node="bar">false</value> </nodes>
    * would return (with attr="node" and parent="nodes") the map [ (foo,true),(bar,false) ]
    *
    * @param enclosingTag the enclosing element
    * @param attr the attribute to retrieve
    * @return ArrayList<TreeMap<String, String>> the keys and values of all children of the
    *    enclosing tags
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public ArrayList<TreeMap<String, String>> getVectorOfTreeMaps(String enclosingTag, String attr)
      throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }
      
      ArrayList<TreeMap<String, String>> outElements = new ArrayList<>(INITIAL_CAPACITY);

      try
      {
         synchronized (this)
         {
            ElementFilter filter = new ElementFilter(enclosingTag);
            for (@SuppressWarnings("unchecked")
               Iterator<Element>  iter = rootElement.getDescendants(filter); iter.hasNext();)
            {
               TreeMap<String, String> map = new TreeMap<>();
               Element node = iter.next();
               
               @SuppressWarnings("unchecked")
               List<Element>  childNodes = node.getChildren();
               for (Element child : childNodes)
               {
                  map.put(child.getAttributeValue(attr), child.getValue());
               }
               outElements.add(map);
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getVectorOfTreeMaps", e);
      }

      return outElements;
   }

   /**
    * Get a vector of all attribute values as TreeMap in the form of ("value", element value) + 
    * (attr name, attr value) + ..., each vector element is a child of the root element
    *
    * Example : <nodes>
    *             <value id="1" type="meta">master1</value>
    *             <value id="2" type="meta">master2</value>
    *           </nodes>
    * would return the vector[ map [ (value,master1), (id,1), (type,meta) ],
    *                          map [ (value,master2), (id,2), (type,meta) ] ]
    *
    * @return ArrayList<TreeMap<String, String>> the keys and values of all children of the
    *    root elements
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public ArrayList<TreeMap<String, String>> getVectorOfAttributeTreeMaps() 
      throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      ArrayList<TreeMap<String, String>> outElements = null;

      try
      {
         synchronized (this)
         {
            @SuppressWarnings("unchecked")
            List<Element> list = rootElement.getChildren();
            outElements = new ArrayList<>(list.size());
            for (Element node : list)
            {
               TreeMap<String, String> map = new TreeMap<>();
               map.put("value", node.getValue());
               
               for (@SuppressWarnings("unchecked")
                  Iterator<Attribute> it = node.getAttributes().iterator(); it.hasNext();)
               {
                  Attribute attr = it.next();
                  String attrName = attr.getName();
                  String attrValue = attr.getValue();
                  map.put(attrName, attrValue);
               }
               outElements.add(map);
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getVectorOfAttributeTreeMaps", e);
      }

      if (outElements == null)
      {
         outElements = new ArrayList<>(0);
      }

      return outElements;
   }

   /**
    * Get a vector of all attribute values as TreeMap in the form of ("value", element value) +
    * (attr name, attr value) + ..., each vector element is a child of the parent element
    *
    * Example : <nodes>
    *             <value id="1" type="meta">master1</value>
    *             <value id="2" type="meta">master2</value>
    *           </nodes>
    * would return the vector[ map [ (value,master1), (id,1), (type,meta) ],
    *                          map [ (value,master2), (id,2), (type,meta) ] ]
    *
    * @return ArrayList<TreeMap<String, String>> the keys and values of all children of the
    *    root elements
    * @throws com.beegfs.admon.common.exceptions.CommunicationException
    */
   public ArrayList<TreeMap<String, String>> getVectorOfAttributeTreeMaps(String parent)
      throws CommunicationException
   {
      if (commError)
      {
         throw new CommunicationException(errorMsg);
      }

      ArrayList<TreeMap<String, String>> outElements = null;

      try
      {
         synchronized (this)
         {
            Element parentElement = rootElement.getChild(parent);
            if (parentElement != null)
            {
               @SuppressWarnings("unchecked")
               List<Element> list = parentElement.getChildren();
               outElements = new ArrayList<>(list.size());
               for (Element node : list)
               {
                  TreeMap<String, String> map = new TreeMap<>();
                  map.put("value", node.getValue());
                  
                  for (@SuppressWarnings("unchecked")
                     Iterator<Attribute> it = node.getAttributes().iterator(); it.hasNext();)
                  {
                     Attribute attr = it.next();
                     String attrName = attr.getName();
                     String attrValue = attr.getValue();
                     map.put(attrName, attrValue);
                  }
                  outElements.add(map);
               }
            }
         }
      }
      catch (Exception e)
      {
         LOGGER.log(Level.SEVERE, "Exception in XML Parser getVectorOfAttributeTreeMaps", e);
      }

      if (outElements == null)
      {
         outElements = new ArrayList<>(0);
      }

      return outElements;
   }
}
