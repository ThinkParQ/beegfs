package com.beegfs.admon.gui.common.enums.quota;



public enum SetQuotaIDListEnum
{
   USER_NAME("User name/ID", "name", 0),
   GROUP_NAME("Group name/ID", "name", 0),
   SIZE_LIMIT("Size limit", "sizeLimit", 1),
   INODE_LIMIT("Chunk file limit", "inodeLimit", 2);


   private static final int TYPE_NUM_VALUES = 1;
   private static final int START_USER_VALUES = 0;
   private static final int START_GROUP_VALUES = TYPE_NUM_VALUES;
   private static final int START_ALL_VALUES = START_GROUP_VALUES + TYPE_NUM_VALUES;

   private final String columnName;
   private final String attributeValue;
   private final int column;

   SetQuotaIDListEnum(String columnName, String attributeValue, int column)
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
      return this.attributeValue;
   }

   public int getColumn()
   {
      return this.column;
   }

   static public int getColumnCount()
   {
      return values().length - 1;
   }

   public static String getAttributeValueByColumn(int column)
   {
      String retVal = "";

      SetQuotaIDListEnum[] types = SetQuotaIDListEnum.values();
      for (SetQuotaIDListEnum type : types)
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
      SetQuotaIDListEnum[] types = SetQuotaIDListEnum.values();
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
