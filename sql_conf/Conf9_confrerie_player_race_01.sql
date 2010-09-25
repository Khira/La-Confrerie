CREATE TABLE `player_race` (
  `entry` int(11) DEFAULT '0',
  `displayid_m` int(11) DEFAULT '0',
  `displayid_w` int(11) DEFAULT '0',
  `scale` float DEFAULT '1',
  `speed` float DEFAULT '1',
  `aura1` int(11) DEFAULT '0',
  `aura2` int(11) DEFAULT '0',
  `aura3` int(11) DEFAULT '0',
  `spell1` int(8) unsigned NOT NULL DEFAULT '0',
  `spell2` int(8) unsigned NOT NULL DEFAULT '0',
  `spell3` int(8) unsigned NOT NULL DEFAULT '0',
  `comments` varchar(255) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8
