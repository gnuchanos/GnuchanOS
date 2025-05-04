-- Load packer
vim.cmd [[packadd packer.nvim]]

-- Install plugins with packer
require('packer').startup(function(use)
  use 'wbthomason/packer.nvim'     -- packer itself
  use 'ms-jpq/coq_nvim'            -- coq autocomplete
  use 'ms-jpq/coq.artifacts'       -- coq sources
  use 'neovim/nvim-lspconfig'      -- lsp client
  use {
    "ray-x/lsp_signature.nvim",    -- LSP signature
    config = function()
      require("lsp_signature").setup{
        floating_window = true,  -- imza penceresi yüzen
        floating_window_above_cur_line = true,  -- hemen üstte olsun
        hint_enable = true,   -- parametre ipuçlarını gösterir
        handler_opts = {
          border = "rounded"  -- yuvarlak kenarlık
        }
      }
    end
  }
end)

-- COQ settings
vim.g.coq_settings = { auto_start = 'shut-up' }

-- Signature helper
local signature_on_attach = require("lsp_signature").on_attach

-- LSP setup
local lspconfig = require('lspconfig')

-- Python LSP
lspconfig.pyright.setup {
  capabilities = require('coq').lsp_ensure_capabilities({}),
  on_attach = signature_on_attach
}

-- C LSP
lspconfig.clangd.setup {
  capabilities = require('coq').lsp_ensure_capabilities({}),
  on_attach = signature_on_attach
}

-- Lua LSP
lspconfig.lua_ls.setup {
  capabilities = require('coq').lsp_ensure_capabilities({}),
  on_attach = signature_on_attach
}

-- Show function signature on Ctrl+Space
vim.api.nvim_set_keymap('i', '<C-Space>', '<Cmd>lua vim.lsp.buf.signature_help()<CR>', { noremap = true, silent = true })

-- Show diagnostic message in the command line when cursor stays on a line
vim.o.updatetime = 250
vim.api.nvim_create_autocmd("CursorHold", {
  callback = function()
    vim.diagnostic.open_float(nil, {
      focusable = false,
      close_events = { "BufLeave", "CursorMoved", "InsertEnter", "FocusLost" },
      border = "rounded",
      source = "always",
      prefix = "⚠️ ",
      scope = "cursor",
    })
  end,
})

-- General Neovim settings
vim.opt.compatible = false
vim.cmd('filetype plugin indent on')

vim.opt.number = true
vim.opt.wrap = false
vim.cmd('syntax on')
vim.opt.termguicolors = true
vim.opt.mouse = 'a'
vim.opt.clipboard = 'unnamedplus'
vim.opt.cmdheight = 2
vim.opt.completeopt = { 'menuone', 'noinsert', 'noselect' }
vim.opt.omnifunc = 'syntaxcomplete#Complete'

vim.g.echodoc_enable_at_startup = 1

-- Key mappings
local map = vim.api.nvim_set_keymap
local opts = { noremap = true, silent = true }

map('n', '<C-n>', ':NERDTreeToggle<CR>', opts)
map('n', '<C-q>', ':q!<CR>', opts)
map('n', '<C-t>', ':tabnew<CR>', opts)
map('n', '<C-A-Right>', ':tabnext<CR>', opts)
map('n', '<C-A-Left>', ':tabprevious<CR>', opts)
map('n', '<A-Left>', ':wincmd h<CR>', opts)
map('n', '<A-Right>', ':wincmd l<CR>', opts)
map('n', '<A-Up>', ':wincmd k<CR>', opts)
map('n', '<A-Down>', ':wincmd j<CR>', opts)
map('n', '<C-Left>', ':vertical resize -2<CR>', opts)
map('n', '<C-Right>', ':vertical resize +2<CR>', opts)
map('n', '<C-Down>', ':resize +2<CR>', opts)
map('n', '<C-Up>', ':resize -2<CR>', opts)

-- Switch to insert mode with backspace key in normal mode
map('n', '<BS>', 'i', opts)

-- Theme and visual tweaks
vim.cmd('highlight CursorLine guibg=#4b0e6e ctermbg=white')

-- Search and display
vim.opt.ignorecase = true
vim.opt.smartcase = true
vim.opt.incsearch = true
vim.opt.hlsearch = true
vim.opt.showcmd = true
vim.opt.ruler = true
vim.opt.showmatch = true
vim.opt.scrolloff = 3
vim.opt.encoding = 'utf-8'
vim.opt.laststatus = 2
vim.opt.showmode = true

-- Tab settings
vim.opt.autoindent = true
vim.opt.smartindent = true
vim.opt.smarttab = true
vim.opt.shiftwidth = 4
vim.opt.softtabstop = 4
vim.opt.tabstop = 4

-- Wildmenu
vim.opt.wildmode = { 'list', 'longest' }
vim.opt.wildmenu = true
vim.opt.wildignore = { '*.o', '*.obj', '*~', '*.png', '*.jpg', '*.gif', 'log/**', 'tmp/**' }
vim.opt.backspace = { 'indent', 'eol', 'start' }

-- Disable true color if needed
vim.opt.termguicolors = false