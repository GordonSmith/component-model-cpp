# <a id="my_game"></a>World my-game


 - Imports:
    - interface `example:my-game/my-plugin-api`
 - Exports:
    - function `run`
    - interface `example:my-game/my-expectations`

## <a id="example_my_game_my_plugin_api"></a>Import interface example:my-game/my-plugin-api


----

### Types

#### <a id="coord"></a>`record coord`


##### Record Fields

- <a id="coord.x"></a>`x`: `u32`
- <a id="coord.y"></a>`y`: `u32`
#### <a id="monster"></a>`record monster`


##### Record Fields

- <a id="monster.name"></a>`name`: `string`
- <a id="monster.hp"></a>`hp`: `u32`
- <a id="monster.pos"></a>`pos`: [`coord`](#coord)
----

### Functions

#### <a id="get_position"></a>`get-position: func`


##### Return values

- <a id="get_position.0"></a> [`coord`](#coord)

#### <a id="set_position"></a>`set-position: func`


##### Params

- <a id="set_position.pos"></a>`pos`: [`coord`](#coord)

#### <a id="monsters"></a>`monsters: func`


##### Return values

- <a id="monsters.0"></a> list<[`monster`](#monster)>

## Exported functions from world `my-game`

#### <a id="run"></a>`run: func`


## <a id="example_my_game_my_expectations"></a>Export interface example:my-game/my-expectations

----

### Types

#### <a id="coord"></a>`record coord`


##### Record Fields

- <a id="coord.x"></a>`x`: `u32`
- <a id="coord.y"></a>`y`: `u32`
#### <a id="monster"></a>`record monster`


##### Record Fields

- <a id="monster.name"></a>`name`: `string`
- <a id="monster.hp"></a>`hp`: `u32`
- <a id="monster.pos"></a>`pos`: [`coord`](#coord)
----

### Functions

#### <a id="get_monster"></a>`get-monster: func`


##### Return values

- <a id="get_monster.0"></a> [`monster`](#monster)

