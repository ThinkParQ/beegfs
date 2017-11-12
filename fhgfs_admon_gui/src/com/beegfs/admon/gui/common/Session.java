/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

package com.beegfs.admon.gui.common;


public class Session {
    private boolean isInfo = false;
    private boolean isAdmin = false;
    private boolean infoAutologinDisabled = true;
    // hashed PW used to login
    private String pw = "";

    public boolean getInfoAutologinDisabled() {
        return infoAutologinDisabled;
    }

    public void setInfoAutologinDisabled(boolean infoAutologinDisabled) {
        this.infoAutologinDisabled = infoAutologinDisabled;
    }

    public String getPw() {
        return pw;
    }

    public void setPw(String pw) {
        this.pw = pw;
    }

    public boolean getIsAdmin() {
        return isAdmin;
    }

    public void setIsAdmin(boolean isAdmin) {
        this.isAdmin = isAdmin;
    }

    public boolean getIsInfo() {
//        return true;
        return (isInfo || isAdmin);
    }

    public void setIsInfo(boolean isInfo) {
        this.isInfo = isInfo;
    }

}
