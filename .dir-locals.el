;;; Use this to configure things (e.g., indentation style) that should be
;;; consistent within our project. Do not do anything that would otherwise
;;; affect how the user interacts with Emacs (e.g., don’t add keybindings or
;;; change “electric” things).
;;;
;;; NB: If you find that some of these variables don’t work for you, we can
;;; change/remove them, but you can also override them by creating a
;;; .dir-locals2.el in this same directory (and add “.dir-locals2.el” to your
;;; ~/.config/git/ignore, so you don’t accidentally commit it).
((nil
  (eval
   ;; This `require` prevents re-evaluation every time we open a buffer. It also
   ;; gives us a bit more breathing room, editor-wise.
   . (require 'electric-coin-company
              (expand-file-name "./contrib/emacs/electric-coin-company.el"
                                (locate-dominating-file default-directory
                                                        ".dir-locals.el"))))
  (c-file-style . "electric-coin-company")
  (compile-command . "./zcutil/build.sh")
  (projectile-project-configure-cmd . "./zcutil/build.sh")
  (projectile-project-package-cmd . "./zcutil/build-debian-package.sh")
  (projectile-project-run-cmd . "./src/zcashd ")
  (projectile-project-test-cmd . "make check")
  (projectile-project-type . make)))
