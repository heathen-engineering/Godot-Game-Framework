extends Node

## The autoload's actual root script. Its whole job is one call: by the time
## _ready() fires on an autoload, Godot guarantees every GDExtension has
## finished loading and the scene tree is genuinely up — a guarantee no
## amount of call_deferred/signal-chaining from GDExtension module init
## could reliably reproduce (both approaches tried and both crashed; see
## SubsystemManagerBridge's register_types.cpp comment for the specifics).
## Autoloads run before the main scene, so every Global subsystem is booted
## (and SubsystemTicker's tick() driving is live) before anything else in
## the game starts relying on one.

func _ready() -> void:
	var bridge = Engine.get_singleton("SubsystemManagerBridge")
	if bridge != null:
		bridge.boot()
