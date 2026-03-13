;; -*- lexical-binding: t; -*-

(TeX-add-style-hook
 "main"
 (lambda ()
   (TeX-add-to-alist 'LaTeX-provided-class-options
                     '(("article" "11pt" "a4paper")))
   (TeX-add-to-alist 'LaTeX-provided-package-options
                     '(("inputenc" "utf8") ("fontenc" "T1") ("babel" "ngerman") ("geometry" "") ("fancyhdr" "") ("graphicx" "") ("tabularx" "") ("booktabs" "") ("xcolor" "") ("titlesec" "") ("listings" "") ("helvet" "")))
   (add-to-list 'LaTeX-verbatim-environments-local "lstlisting")
   (add-to-list 'LaTeX-verbatim-macros-with-braces-local "lstinline")
   (add-to-list 'LaTeX-verbatim-macros-with-delims-local "lstinline")
   (TeX-run-style-hooks
    "latex2e"
    "preamble"
    "tasks/theory"
    "tasks/task_a"
    "tasks/task_b"
    "tasks/task_c"
    "tasks/task_d"
    "tasks/task_e"
    "article"
    "art11"))
 :latex)

