package com.beegfs.admon.gui.common.enums.quota;



public enum GetQuotaEnum
{
   USER_NAME("User name", "name", 0),
   USER_ID("User ID", "ID", 1),
   GROUP_NAME("Group name", "name", 0),
   GROUP_ID("Group ID", "ID", 1),
   SIZE_USED("Used size", "usedSize",2 ),
   SIZE_LIMIT("Size limit", "sizeLimit", 3),
   INODE_SIZE("Used chunk files", "usedInodes", 4),
   INODE_LIMIT("Chunk file limit", "inodeLimit", 5);


   private static final int TYPE_NUM_VALUES = 2;
   private static final int START_USER_VALUES = 0;
   private static final int START_GROUP_VALUES = TYPE_NUM_VALUES;
   private static final int START_ALL_VALUES = START_GROUP_VALUES + TYPE_NUM_VALUES;

   private final String columnName;
   private final String attributeValue;
   private final int column;

   GetQuotaEnum(String columnName, String attributeValue, int column)
   {
      this.columnName = columnName;
      this.attributeValue = attributeValue;
      this.column = column;
   }

   public String getColumnName()
   {
      return this.columnName;
   }

   public String getAttributeValue()
   {
      return this.attributeValue;
   }

   public int getColumn()
   {
      return this.column;
   }

   static public int getColumnCount()
   {
      return values().length - 2;
   }

   static public String[] getColumnNames(QuotaIDTypeEnum type)
   {
      GetQuotaEnum[] types = GetQuotaEnum.values();
      String[] retVal = new String[getColumnCount()];

      if (type == QuotaIDTypeEnum.USER)
      {
         for (int index = START_USER_VALUES; index < START_USER_VALUES + TYPE_NUM_VALUES; index++)
         {
            retVal[types[index].getColumn()] = types[index].toString();
         }
      }
      else
      {
         for (int index = START_GROUP_VALUES; index < START_GROUP_VALUES + TYPE_NUM_VALUES; index++)
         {
            retVal[types[index].getColumn()] = types[index].toString();
         }
      }

      for (int index = START_ALL_VALUES; index < getColumnCount(); index++)
      {
         retVal[types[index].getColumn()] = types[index].toString();
      }

      return retVal;
   }
}
