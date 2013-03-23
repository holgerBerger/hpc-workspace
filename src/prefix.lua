-- simple prefix extractor example, takes the component in front of the username from a users home
require "posix"
function prefix(filesystem, username)
    pw = posix.getpasswd(username)
    return string.match(pw.dir, "/(%a+)/" .. username)
end
