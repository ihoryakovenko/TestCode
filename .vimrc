call plug#begin('~/.vim/plugged')
Plug 'preservim/nerdtree'
Plug 'tpope/vim-fugitive'
Plug 'vim-airline/vim-airline'
call plug#end()

set hidden
set splitright
set splitbelow
set number             " Show line numbers on the left

syntax on           " Enable syntax highlighting
filetype plugin on  " Enable filetype detection

let g:airline#extensions#tabline#enabled = 1 " Enable the list of buffers
let g:airline#extensions#tabline#fnamemod = ':t' " Show just the filename

nnoremap <leader>s :w<CR>  " Save file with Leader + 
nnoremap <Tab> :bnext<CR> " Move to the next buffer with Tab
nnoremap <S-Tab> :bprevious<CR " Move to the previous buffer with Shift+Tab
nnoremap <leader>qq :q<CR>      " Quit with Leader + qq
nnoremap <leader>q :bp \| bdelete #<CR>  " Close current buffer
nnoremap <leader>c :Gvdiffsplit<CR>

function! OpenInPreviousSplit()
    if !exists('g:NERDTree') || !g:NERDTree.IsOpen()
        return
    endif

    let node = g:NERDTreeFileNode.GetSelected()
    if empty(node)
        return
    endif

    if node.path.isDirectory
        call nerdtree#ui_glue#invokeKeyMap('<CR>')
        return
    endif

	wincmd p  " Switch to the last active split
	let filepath = fnameescape(node.path.str())
	execute 'edit' filepath
endfunction

augroup nerdtree_custom_splits
  autocmd!
  autocmd FileType nerdtree nnoremap <buffer> <CR> :call OpenInPreviousSplit()<CR>
augroup END

