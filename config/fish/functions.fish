# Create directory and jump into it
function mkcd
    mkdir -p $argv[1]
    and cd $argv[1]
end

function addpk
  echo $argv[1] | tee -a ~/.local/share/chezmoi/.install_pkg.lst 
  and sort -u -o ~/.local/share/chezmoi/.install_pkg.lst ~/.local/share/chezmoi/.install_pkg.lst  
end

function git-clone-and-cd
  git clone $argv[1] $argv[2]
  cd $(test -n "$argv[2]"; and echo $argv[2]; or echo $argv[1] | sed 's/^.*\///;s/\.git//')
end


function git-clone-and-cd-fast
  git clone --depth 1 --single-branch --filter=blob:none $argv[1] $argv[2]
  cd $(test -n "$argv[2]"; and echo $argv[2]; or echo $argv[1] | sed 's/^.*\///;s/\.git//')
end


# Turn ... into cd ../../
function multicd
    echo cd (string repeat -n (math (string length -- $argv[1]) - 1) ../)
end
abbr --add dotdot --regex '^\.\.+$' --function multicd

# auto-configm yay updates if there are no problems
function yayu
    expect -c "
    spawn yay -Syu --devel 
    expect_background -ex \"Proceed with installation\" {send \"y\\n\"}
    interact
	"
end

function y
	set tmp (mktemp -t "yazi-cwd.XXXXXX")

	if test -n "$KITTY_PID"
	  kitty @ set-spacing padding=0 margin=0
	end

	yazi $argv --cwd-file="$tmp"

	if test -n "$KITTY_PID"
	  kitty @ set-spacing padding=24 margin=0
    end

	if set cwd (command cat -- "$tmp"); and [ -n "$cwd" ]; and [ "$cwd" != "$PWD" ]
		# Use the normal cd here as it is actually zoxide and then zoxide remembers
		cd -- "$cwd"
	end

	rm -f -- "$tmp"
end

function gitm
    # Check if we're in a Git repository
    if not git rev-parse --is-inside-work-tree >/dev/null 2>&1
        echo "Not in a Git repository."
        return 1
    end

    # Try to determine the primary branch (main or master)
    set branch (git symbolic-ref refs/remotes/origin/HEAD 2>/dev/null | sed 's@^refs/remotes/origin/@@')
    
    # If neither main nor master is found, default to master
    if test -z "$branch"
        set branch master
    end

    # Checkout the primary branch
    git checkout $branch
end

function cdn 
	cd $argv[1] && nvim .
end
