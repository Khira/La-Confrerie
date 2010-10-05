DROP TABLE IF EXISTS `player_seller`;
CREATE TABLE `player_seller` (
  `pguid` INT(11) DEFAULT '0',
  `vendorTypeFlag` INT(10) UNSIGNED NOT NULL DEFAULT '0',
  `itemLevelMax` MEDIUMINT(8) UNSIGNED NOT NULL DEFAULT '0',
  `itemQualityMax` MEDIUMINT(8) UNSIGNED NOT NULL DEFAULT '0',
  `itemRequierd` MEDIUMINT(8) UNSIGNED NOT NULL DEFAULT '0',
  PRIMARY KEY  (`pguid`)
) ENGINE=MYISAM DEFAULT CHARSET=utf8 COMMENT='Seller System';
