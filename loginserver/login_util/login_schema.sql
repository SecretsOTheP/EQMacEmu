DROP TABLE IF EXISTS tblloginserveraccounts;
CREATE TABLE IF NOT EXISTS tblloginserveraccounts (
  LoginServerID integer unsigned NOT NULL auto_increment,
  AccountName varchar(30) NOT NULL,
  AccountPassword varchar(50) NOT NULL,
  AccountCreateDate timestamp default CURRENT_TIMESTAMP NOT NULL,
  AccountEmail varchar(100) NOT NULL,
  LastLoginDate datetime NOT NULL,
  LastIPAddress varchar(15) NOT NULL,
  PRIMARY KEY (LoginServerID, AccountName)
) ENGINE=InnoDB;

DROP TABLE IF EXISTS tblserverlisttype;
CREATE TABLE IF NOT EXISTS tblserverlisttype (
	ServerListTypeID integer unsigned NOT NULL,
	ServerListTypeDescription varchar(20) NOT NULL,
	PRIMARY KEY (ServerListTypeID)
) ENGINE=MyISAM;

INSERT INTO tblserverlisttype (ServerListTypeID, ServerListTypeDescription) VALUES (1, 'Legends');
INSERT INTO tblserverlisttype (ServerListTypeID, ServerListTypeDescription) VALUES (2, 'Preferred');
INSERT INTO tblserverlisttype (ServerListTypeID, ServerListTypeDescription) VALUES (3, 'Standard');

DROP TABLE IF EXISTS tblserveradminregistration;
CREATE TABLE IF NOT EXISTS tblserveradminregistration (
	ServerAdminID integer unsigned NOT NULL auto_increment,
	AccountName varchar(30) NOT NULL,
	AccountPassword varchar(30) NOT NULL,
	FirstName varchar(40) NOT NULL,
	LastName varchar(50) NOT NULL,
	Email varchar(100) NULL,
	RegistrationDate datetime NOT NULL,
	RegistrationIPAddr varchar(15) NOT NULL,
	PRIMARY KEY (ServerAdminID, Email)
) ENGINE=MyISAM;

DROP TABLE IF EXISTS tblworldserverregistration;
CREATE TABLE IF NOT EXISTS tblworldserverregistration (
  ServerID integer unsigned NOT NULL auto_increment,
  ServerLongName varchar(100) NOT NULL,
  ServerTagDescription varchar(50) NOT NULL DEFAULT '',
  ServerShortName varchar(25) NOT NULL,
  ServerListTypeID integer NOT NULL,
  ServerLastLoginDate datetime NULL,
  ServerLastIPAddr varchar(15) NULL,
  ServerAdminID integer NOT NULL,
  ServerTrusted integer NOT NULL,
  Note varchar(300) NULL,
  PRIMARY KEY (ServerID, ServerLongName)
) ENGINE=InnoDB;

DROP TABLE IF EXISTS tblloginserversettings;
CREATE TABLE `tblloginserversettings` (
	`type` VARCHAR(50) NOT NULL COLLATE 'latin1_swedish_ci',
	`value` VARCHAR(50) NULL DEFAULT NULL COLLATE 'latin1_swedish_ci',
	`category` VARCHAR(20) NOT NULL COLLATE 'latin1_swedish_ci',
	`description` VARCHAR(99) NOT NULL COLLATE 'latin1_swedish_ci',
	`defaults` VARCHAR(50) NULL DEFAULT NULL COLLATE 'latin1_swedish_ci'
)
COLLATE='latin1_swedish_ci'
ENGINE=InnoDB
;

INSERT INTO `tblloginserversettings` VALUES ('allow_PCT', 'TRUE', 'options', 'allow PC ticketed client', 'TRUE');
INSERT INTO `tblloginserversettings` VALUES ('allow_OSX', 'TRUE', 'options', 'allow OSX client', 'TRUE');
INSERT INTO `tblloginserversettings` VALUES ('allow_PC', 'TRUE', 'options', 'allow PC client', 'TRUE');
INSERT INTO `tblloginserversettings` VALUES ('pop_count', '0', 'options', '0 to only display UP or DOWN or 1 to show population count in server select.', '0');
INSERT INTO `tblloginserversettings` VALUES ('ticker', 'Welcome to AkkStack EQMacEmu', 'options', 'Sets the welcome message in server select.', 'Welcome to AkkStack EQMacEmu');