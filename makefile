factory: factory.c carton cool bake
	gcc factory.c -o factory 

carton: carton.c
	gcc carton.c -o carton 

cool: cool.c
	gcc cool.c -o cool

bake: bake.c
	gcc bake.c -o bake
