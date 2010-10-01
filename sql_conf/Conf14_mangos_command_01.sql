DELETE FROM `command` WHERE `name` IN ('confrerie morph create');

INSERT INTO `command` (`name`, `security`, `help`) VALUES ('confrerie morph create', 4, 'Syntaxe: .confrerie morph create "$nameDesc"\nPermet de créer une nouvelle race spéciale avec comme nom/commentaire $nameDesc. Utilisez .confrerie morph edit pour modifier les options de la race.');
