package com.beegfs.admon.gui.components.lf;

import com.nilo.plaf.nimrod.NimRODTheme;
import java.awt.Color;
import java.awt.Font;

public class BeegfsLookAndFeel extends com.nilo.plaf.nimrod.NimRODLookAndFeel
{
   private static final long serialVersionUID = 1L;

   private final transient NimRODTheme beegfsTheme;

   public BeegfsLookAndFeel()
   {
      beegfsTheme = new NimRODTheme();
      // do not change the order of color definition, this will have an impact to the color settings
      //Color.decode("#FDB813")
      beegfsTheme.setPrimary1(Color.decode("#747670"));
      beegfsTheme.setPrimary2(Color.decode("#7E807A"));
      beegfsTheme.setPrimary3(Color.decode("#888A84"));
      beegfsTheme.setPrimary(Color.decode("#FDB813"));    //selection color
      beegfsTheme.setSecondary1(Color.decode("#D9D7AD"));
      beegfsTheme.setSecondary2(Color.decode("#E3E1B7"));
      beegfsTheme.setSecondary3(Color.decode("#EDEBC1"));
      beegfsTheme.setSecondary(Color.decode("#F6F4F4"));  //background color
      beegfsTheme.setWhite(Color.decode("#FFFFFF"));
      beegfsTheme.setBlack(Color.decode("#000000"));
      beegfsTheme.setMenuOpacity(255);
      beegfsTheme.setFrameOpacity(255);
      beegfsTheme.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 12));

      BeegfsLookAndFeel.setCurrentTheme(beegfsTheme);
   }
}
