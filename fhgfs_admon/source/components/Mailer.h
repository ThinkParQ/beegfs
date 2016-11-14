#ifndef MAILER_H_
#define MAILER_H_

#include <common/Common.h>
#include <components/ExternalJobRunner.h>

struct DownNode
{
   NumNodeID nodeNumID;
   std::string nodeID;
   int64_t downSince;
   bool eMailSent;
};

typedef std::list<DownNode> DownNodeList;
typedef std::list<DownNode>::iterator DownNodeListIter;

struct DownNodesContainer
{
      DownNodeList downMetaNodes;
      DownNodeList downStorageNodes;
      int64_t lastMail;
      bool newDownNodes;
};

class Mailer : public PThread
{
   public:
      Mailer() : PThread("Mailer"), log("Mailer") {};
      virtual ~Mailer() {};

      static int sendMail(const std::string& smtpServerName, const std::string& sender,
         const std::string& recipient, const std::string& subject, const std::string& msg);
      bool addDownNode(NumNodeID nodeNumID, std::string nodeID, NodeType nodeType);


   protected:
      LogContext log;


   private:
      DownNodesContainer downNodes;

      static std::string intToWeekday(int intWeekday)
      {
         std::string outWeekday;
         switch (intWeekday)
         {
            case 0: outWeekday="Sun"; break;
            case 1: outWeekday="Mon"; break;
            case 2: outWeekday="Tue"; break;
            case 3: outWeekday="Wed"; break;
            case 4: outWeekday="Thu"; break;
            case 5: outWeekday="Fri"; break;
            case 6: outWeekday="Sat"; break;
         }

         return outWeekday;
      }

      static std::string intToMonth(int intMonth)
      {
         std::string outMonth;
         switch (intMonth)
         {
            case 0 : outMonth="Jan";break;
            case 1 : outMonth="Feb";break;
            case 2 : outMonth="Mar";break;
            case 3 : outMonth="Apr";break;
            case 4 : outMonth="May";break;
            case 5 : outMonth="Jun";break;
            case 6 : outMonth="Jul";break;
            case 7 : outMonth="Aug";break;
            case 8 : outMonth="Sep";break;
            case 9 : outMonth="Oct";break;
            case 10 : outMonth="Nov";break;
            case 11 : outMonth="Dec";break;

         }

         return outMonth;
      }

      void run();
      void notifyDownNodes();
      void notifyBackUpNodes();

   public:
      // public inliners

      static std::string getHumanReadableNodeID(DownNode downNode)
      {
         return downNode.nodeID + " [ID: " + downNode.nodeNumID.str() + "]";
      }
};

#endif /*MAILER_H_*/
