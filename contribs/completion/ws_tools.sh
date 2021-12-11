#
# bash completion for hpc-workspace
#
# Copyright(c) 2020-2021 Christoph Niethammer
#
#
# Note: Currently only the completions for ws_find and ws_share take a
#       provided filesystems into account when completing workspace names.
#
#
# bash completion for hpc-workspace is free software: you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# bash completion for hpc-workspaceis is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# bash completion for hpc-workspace.  If not, see <http://www.gnu.org/licenses/>.
#



function _ws_filesystem_list() {
    local file_systems="$(ws_list -l | tail -n +2 | cut -f 1 -d ' ')"
    printf "%s" "$file_systems"
}

function _ws_workspace_list() {
    local file_system="$1"
    if [ "$file_system" != "" ] ; then
        ws_names="$(ws_list -F "$file_system" -s 2>/dev/null)"
    else
        ws_names="$(ws_list -s)"
    fi
    printf "%s" "$ws_names"
}

function _ws_restore_list() {
    local restore_targets="$(ws_restore -l -b)"
    printf "%s" "$restore_targets"
}

# ws_find completion
function _complete_ws_find() {
    case "${COMP_WORDS[$COMP_CWORD]}" in
        -*)
            [[ "$COMP_CWORD" != "1" ]] && return
            COMPREPLY=($(compgen -W "-F --filesystem -h -l --list" -- "${COMP_WORDS[$COMP_CWORD]}"))
            ;;
        *)
            if [[ "${COMP_WORDS[1]}" == "-F" || "${COMP_WORDS[1]}" == "--filesystem" ]] ; then
                file_system="${COMP_WORDS[2]}"
            fi
            case "${COMP_WORDS[$COMP_CWORD - 1]}" in
                -F|--filesystem)
                    local file_systems="$(_ws_filesystem_list)"
                    COMPREPLY=($(compgen -W "$file_systems" -- "${COMP_WORDS[$COMP_CWORD]}"))
                    ;;
                *)
                    [[ ! ("$COMP_CWORD" == "1" || ("$file_system" != "" && "$COMP_CWORD" == "3")) ]] && return
                    local ws_names=$(_ws_workspace_list "$file_system")
                    COMPREPLY=($(compgen -W "$ws_names" -- "${COMP_WORDS[$COMP_CWORD]}"))
                    ;;
            esac
    esac
}
complete -F _complete_ws_find ws_find


# ws_list completion
function _complete_ws_list() {
    case "${COMP_WORDS[$COMP_CWORD - 1]}" in
        -F|--filesystem)
            local file_systems="$(_ws_filesystem_list)"
            COMPREPLY=($(compgen -W "$file_systems" -- "${COMP_WORDS[$COMP_CWORD]}"))
            ;;
        *)
            COMPREPLY=($(compgen -W "-a -C -F --filesystem -g -h -l -N -r -R -s -t -v" -- "${COMP_WORDS[$COMP_CWORD]}"))
            ;;
    esac
}
complete -F _complete_ws_list ws_list


# ws_extend completion with available workspace names
function _complete_ws_extend() {
    COMPREPLY=($(compgen -W "$(ws_list -s)" -- "${COMP_WORDS[$COMP_CWORD]}"))
}
complete -F _complete_ws_extend ws_extend


# ws_release completion with available workspace names
function _complete_ws_release() {
    COMPREPLY=($(compgen -W "$(ws_list -s)" -- "${COMP_WORDS[$COMP_CWORD]}"))
}
complete -F _complete_ws_release ws_release


# ws_restore completion with available workspace names
function _complete_ws_restore() {
    case "${COMP_WORDS[$COMP_CWORD]}" in
        -*)
            COMPREPLY=($(compgen -W "--brief --filesystem --help --list --name --target --username --version" -- "${COMP_WORDS[$COMP_CWORD]}"))
            ;;
        *)
            prev="${COMP_WORDS[$COMP_CWORD - 1]}"
            local restore_targets="$(_ws_restore_list)"
            if [[ "$prev" == "-F" || "$prev" == "--filesystem" ]] ; then
                local file_systems="$(_ws_filesystem_list)"
                COMPREPLY=($(compgen -W "$file_systems" -- "${COMP_WORDS[$COMP_CWORD]}"))
            elif [[ "$prev" == "--name" || " $restore_targets " =~ " $prev " ]] ; then
                COMPREPLY=($(compgen -W "$(ws_list -s)" -- "${COMP_WORDS[$COMP_CWORD]}"))
            else
                COMPREPLY=($(compgen -W "$restore_targets" -- "${COMP_WORDS[$COMP_CWORD]}"))
            fi
            ;;
    esac
}
complete -F _complete_ws_restore ws_restore


# ws_share completion with available workspace names and system users
function _complete_ws_share() {
    case "${COMP_WORDS[$COMP_CWORD]}" in
        -*)
            COMPREPLY=($(compgen -W "--filesystem --help" -- "${COMP_WORDS[$COMP_CWORD]}"))
            ;;
        *)
            prev="${COMP_WORDS[$COMP_CWORD - 1]}"
            if [[ "$COMP_CWORD" == "1" ]] ; then
                COMPREPLY=($(compgen -W "share unshare unshare-all list" -- "${COMP_WORDS[$COMP_CWORD]}"))
            elif [[ "$prev" == "-F" || "$prev" == "--filesystem" ]] ; then
                local file_systems="$(_ws_filesystem_list)"
                COMPREPLY=($(compgen -W "$file_systems" -- "${COMP_WORDS[3]}"))
            else
                action="${COMP_WORDS[1]}"
                declare -i argpos=$((COMP_CWORD - 1))
                if [[ "${COMP_WORDS[2]}" == "-F" || "${COMP_WORDS[2]}" == "--filesystem" ]] ; then
                    file_system="${COMP_WORDS[3]}"
                    argpos=$((argpos - 2))
                fi
                case $action in
                    share)
                        case $argpos in
                            1)
                                local ws_names=$(_ws_workspace_list "$file_system")
                                COMPREPLY=($(compgen -W "$ws_names" -- "${COMP_WORDS[$COMP_CWORD]}"))
                                ;;
                            *)
                                COMPREPLY=($(compgen -A user -- "${COMP_WORDS[$COMP_CWORD]}"))
                                ;;
                        esac
                        ;;
                    unshare)
                        case $argpos in
                            1)
                                local ws_names=$(_ws_workspace_list "$file_system")
                                COMPREPLY=($(compgen -W "$ws_names" -- "${COMP_WORDS[$COMP_CWORD]}"))
                                ;;
                            *)
                                ws_name="${COMP_WORDS[$((COMP_CWORD - argpos + 1))]}"
                                filesysopt="${filesystem:+-F $filesystem}"
                                COMPREPLY=($(compgen -W "$(ws_share list $filesysopt $ws_name)" -- "${COMP_WORDS[$COMP_CWORD]}"))
                                ;;
                        esac
                        ;;
                    unshare-all|list)
                        if [[ $argpos -le 1 ]] ; then
                            local ws_names=$(_ws_workspace_list "$file_system")
                            COMPREPLY=($(compgen -W "$ws_names" -- "${COMP_WORDS[$COMP_CWORD]}"))
                        else
                            COMPREPLY=""
                        fi
                        ;;
                esac
            fi
            ;;
    esac
}
complete -F _complete_ws_share ws_share

