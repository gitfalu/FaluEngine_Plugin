local speed = 3.0

function onInit(entity)
    log_info("Lua onInit: " .. entity:getName())
end

function onUpdate(entity, dt)
    local transform = entity:getTransform()

    if Input.isKeyDown("W") then
        transform.position.z = transform.position.z + speed * dt
    end
    if Input.isKeyDown("S") then
        transform.position.z = transform.position.z - speed * dt
    end
    if Input.isKeyDown("A") then
        transform.position.x = transform.position.x - speed * dt
    end
    if Input.isKeyDown("D") then
        transform.position.x = transform.position.x + speed * dt
    end
end


function onDestroy(entity)
    log_info("Lua onDestroy: " .. entity:getName())
end