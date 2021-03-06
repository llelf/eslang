#include <stdio.h>
#include <string.h>
#include <slang.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#ifndef MAP_FAILED
# define MAP_FAILED	-1
#endif

SLANG_MODULE(varray);

typedef struct
{
   unsigned long size_mmapped;
   VOID_STAR addr;
   VOID_STAR data;
}
MMap_Type;

static void free_mmap_type (MMap_Type *m)
{
   if (m == NULL)
     return;
   if (m->addr != NULL)
     (void) munmap ((char *) m->addr, m->size_mmapped);
   SLfree ((char *)m);
}

static void unmmap_array (SLang_Array_Type *at)
{
   if (at->client_data != NULL)
     free_mmap_type ((MMap_Type *) at->client_data);

   at->data = NULL;
   at->client_data = NULL;
}

     
static MMap_Type *mmap_file (char *file, unsigned int offset, 
			     unsigned long num_bytes)
{
   FILE *fp;
   int fd;
   struct stat st;
   VOID_STAR addr;
   MMap_Type *m;

   fp = fopen (file, "rb");
   if (fp == NULL)
     {
	SLang_verror (SL_OBJ_NOPEN, "mmap_array: unable to open %s for reading", file);
	return NULL;
     }
   fd = fileno (fp);
   
   if (-1 == fstat (fd, &st))
     {
	SLang_verror (SL_INTRINSIC_ERROR, "mmap_array: stat %s failed", file);
	fclose (fp);
	return NULL;
     }
   
   if (NULL == (m = (MMap_Type *) SLmalloc (sizeof (MMap_Type))))
     {
	fclose (fp);
	return NULL;
     }

   m->size_mmapped = num_bytes + offset;
   addr = (VOID_STAR)mmap (NULL, m->size_mmapped, PROT_READ, MAP_SHARED, fd, 0);
   if (addr == (VOID_STAR)MAP_FAILED)
     {
	SLang_verror (SL_INTRINSIC_ERROR, "mmap_array: mmap %s failed", file);
	SLfree ((char *) m);
	fclose (fp);
	return NULL;
     }
   m->addr = addr;
   m->data = (VOID_STAR) ((char *)addr + offset);

   fclose (fp);
   
   return m;
}

/* usage:
 *  a = mmap_array (file, offset, type, [dims]);
 */
static void mmap_array (void)
{
   SLang_Array_Type *a, *a_dims;
   char *file;
   unsigned char type;
   int *dims;
   unsigned int num_dims;
   unsigned int i;
   unsigned int num_elements;
   unsigned int offset;
   unsigned int sizeof_type;
   unsigned long num_bytes;
   MMap_Type *m;
   VOID_STAR data;

   a_dims = NULL;
   file = NULL;
   data = NULL;

   if (-1 == SLang_pop_array_of_type (&a_dims, SLANG_INT_TYPE))
     return;
   
   num_dims = a_dims->num_elements;
   dims = (int *)a_dims->data;
   
   if (-1 == SLang_pop_datatype (&type))
     goto return_error;
   
   switch (type)
     {
      case SLANG_CHAR_TYPE:
      case SLANG_UCHAR_TYPE:
	sizeof_type = 1;
	break;

      case SLANG_SHORT_TYPE:
      case SLANG_USHORT_TYPE:
	sizeof_type = sizeof(short);
	break;

      case SLANG_INT_TYPE:
      case SLANG_UINT_TYPE:
	sizeof_type = sizeof (int);
	break;

      case SLANG_LONG_TYPE:
      case SLANG_ULONG_TYPE:
	sizeof_type = sizeof (long);
	break;
	 
      case SLANG_FLOAT_TYPE:
	sizeof_type = sizeof (float);
	break;

      case SLANG_DOUBLE_TYPE:
	sizeof_type = sizeof (double);
	break;

      case SLANG_COMPLEX_TYPE:
	sizeof_type = 2 * sizeof (double);
	break;
	
      default:
	SLang_verror (SL_NOT_IMPLEMENTED, "mmap_array: unsupported data type");
	goto return_error;
     }
   
   num_elements = 1;
   for (i = 0; i < num_dims; i++)
     {
	if (dims[i] < 0)
	  {
	     SLang_verror (SL_USER_ERROR, "mmap_array: dims array must be positive");
	     goto return_error;
	  }
	
	num_elements *= dims[i];
     }
   if (num_dims == 0)
     num_elements = 0;

   num_bytes = (unsigned long) sizeof_type * (unsigned long) num_elements;

   if (-1 == SLang_pop_uinteger (&offset))
     goto return_error;

   if (-1 == SLang_pop_slstring (&file))
     goto return_error;

   if (NULL == (m = mmap_file (file, offset, num_bytes)))
     goto return_error;

   if (NULL == (a = SLang_create_array (type, 1, m->data, dims, num_dims)))
     goto return_error;

   a->free_fun = unmmap_array;
   a->client_data = (VOID_STAR) m;

   m = NULL;			       /* done with this */
   
   (void) SLang_push_array (a, 1);

   /* drop */

   return_error:
   if (m != NULL)
     free_mmap_type (m);
   if (a_dims != NULL)
     SLang_free_array (a_dims);
   if (file != NULL)
     SLang_free_slstring (file);
}

static SLang_Intrin_Fun_Type Module_Intrinsics [] =
{
   MAKE_INTRINSIC_0("mmap_array", mmap_array, SLANG_VOID_TYPE),
   SLANG_END_INTRIN_FUN_TABLE
};


int init_varray_module_ns (char *ns_name)
{
   SLang_NameSpace_Type *ns;
   
   if (NULL == (ns = SLns_create_namespace (ns_name)))
     return -1;

   if (-1 == SLns_add_intrin_fun_table (ns, Module_Intrinsics, NULL))
     return -1;

   return 0;
}

