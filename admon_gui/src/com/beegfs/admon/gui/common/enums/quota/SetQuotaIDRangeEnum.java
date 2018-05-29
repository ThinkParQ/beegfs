package com.beegfs.admon.gui.common.enums.quota;



public enum SetQuotaIDRangeEnum
{
   USER_ID_RANGE_START("User ID range start", "rangeStart", 0),
   USER_ID_RANGE_END("User ID range end", "rangeEnd", 1),
   GROUP_ID_RANGE_START("Group ID range start", "rangeStart", 0),
   GROUP_ID_RANGE_END("Group ID range end", "rangeEnd", 1),
   SIZE_LIMIT("Size limit", "sizeLimit", 2),
   INODE_LIMIT("Chunk file limit", "inodeLimit", 3);


   private static final int TYPE_NUM_VALUES = 2;
   private static final int START_USER_VALUES = 0;
   private static final int START_GROUP_VALUES = TYPE_NUM_VALUES;
   private static final int START_ALL_VALUES = START_GROUP_VALUES + TYPE_NUM_VALUES;

   private final String columnName;
   private final String attributeValue;
   private final int column;

   SetQuotaIDRangeEnum(String columnName, String attributeValue, int column)
   {
      this.columnName = columnName;
      this.attributeValue = attributeValue;
      this.column = column;
   }

   public String getColumnName()
   {
      return columnName;
   }

   public String getAttributeValue()
   {
      return attributeValue;
   }

   public int getColumn()
   {
      return this.column;
   }

   static public int getColumnCount()
   {
      return values().length - 2;
   }

   public static String getAttributeValueByColumn(int column)
   {
      String retVal = "";

      SetQuotaIDRangeEnum[] types = SetQuotaIDRangeEnum.values();
      for (SetQuotaIDRangeEnum type : types)
      {
         if (column == type.getColumn() )
         {
            retVal = type.getAttributeValue();
         }
      }

      return retVal;
   }

   static public String[] getColumnNames(QuotaIDTypeEnum type)
   {
      SetQuotaIDRangeEnum[] types = SetQuotaIDRangeEnum.values();
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
