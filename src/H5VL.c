/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:	The Virtual Object Layer as described in documentation.
 *              The pupose is to provide an abstraction on how to access the
 *              underlying HDF5 container, whether in a local file with
 *              a specific file format, or remotely on other machines, etc...
 */

/****************/
/* Module Setup */
/****************/

#include "H5VLmodule.h"         /* This source code file is part of the H5VL module */


/***********/
/* Headers */
/***********/

#include "H5private.h"          /* Generic Functions                    */
#include "H5Aprivate.h"         /* Attributes                           */
#include "H5Eprivate.h"         /* Error handling                       */
#include "H5Iprivate.h"         /* IDs                                  */
#include "H5MMprivate.h"        /* Memory management                    */
#include "H5PLprivate.h"        /* Plugins                              */
#include "H5VLpkg.h"            /* Virtual Object Layer                 */


/****************/
/* Local Macros */
/****************/

/******************/
/* Local Typedefs */
/******************/

/* Information needed for iterating over the registered VOL driver hid_t IDs.
 * The name of the new VOL driver that is being registered is stored in the
 * name field and the found_id field is initialized to H5I_INVALID_HID (-1).
 * If we find a VOL driver with the same name, we set the found_id field to
 * the existing ID for return to the function.
 */
typedef struct {
    const char *name;           /* The name of the VOL driver to check */
    hid_t found_id;             /* The library ID if we found a match */
} H5VL_get_driver_ud_t;

/********************/
/* Local Prototypes */
/********************/

/*********************/
/* Package Variables */
/*********************/

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/


/*-------------------------------------------------------------------------
 * Function:    H5VL__get_driver_cb
 *
 * Purpose:     Callback routine to search through registered VOLs
 *
 * Return:      Success:    H5_ITER_STOP if the class and op_data name
 *                          members match. H5_ITER_CONT otherwise.
 *
 *              Failure:    Can't fail
 *
 *-------------------------------------------------------------------------
 */
static int
H5VL__get_driver_cb(void *obj, hid_t id, void *_op_data)
{
    H5VL_get_driver_ud_t *op_data = (H5VL_get_driver_ud_t *)_op_data; /* User data for callback */
    H5VL_class_t *cls = (H5VL_class_t *)obj;
    int ret_value = H5_ITER_CONT;     /* Callback return value */

    FUNC_ENTER_STATIC_NOERR

    if(0 == HDstrcmp(cls->name, op_data->name)) {
        op_data->found_id = id;
        ret_value = H5_ITER_STOP;
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_driver_cb() */


/*-------------------------------------------------------------------------
 * Function:    H5VLregister
 *
 * Purpose:     Registers a new vol driver as a member of the virtual object
 *              layer class.
 *
 * Return:      Success:    A vol driver ID which is good until the
 *                          library is closed or the driver is
 *                          unregistered.
 *
 *              Failure:    A negative value (H5I_INVALID_HID).
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VLregister(const H5VL_class_t *cls)
{
    H5VL_get_driver_ud_t op_data;
    hid_t ret_value = H5I_INVALID_HID;

    FUNC_ENTER_API(FAIL)
    H5TRACE1("i", "*x", cls);

    /* Check arguments */
    if(!cls)
        HGOTO_ERROR(H5E_ARGS, H5E_UNINITIALIZED, H5I_INVALID_HID, "VOL driver class pointer cannot be NULL")
    if(!cls->name)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "VOL driver class name cannot be the NULL pointer");
    /* XXX: Should probably come up with a max length so we can use strnlen()? */
    if(0 == HDstrlen(cls->name))
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "VOL driver class name cannot be the empty string");

    op_data.found_id = H5I_INVALID_HID;
    op_data.name = cls->name;

    /* check if driver is already registered */
    if(H5I_iterate(H5I_VOL, H5VL__get_driver_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't iterate over VOL IDs")

    /* XXX: Does this need to be an error? Users probably don't care as long
     *      as their VOL driver is available.
     */
    if(op_data.found_id != H5I_INVALID_HID)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "VOL driver with the same name is already registered.")

    /* Create the new class ID */
    if((ret_value = H5VL_register(cls, sizeof(H5VL_class_t), TRUE)) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register VOL driver")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLregister() */


/*-------------------------------------------------------------------------
 * Function:    H5VLregister_by_name
 *
 * Purpose:     Registers a new vol driver as a member of the virtual object
 *              layer class.
 *
 * Return:      Success:    A vol driver ID which is good until the
 *                          library is closed or the driver is
 *                          unregistered.
 *
 *              Failure:    A negative value.
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VLregister_by_name(const char *name)
{
    hid_t ret_value = H5I_INVALID_HID;

    FUNC_ENTER_API(FAIL)
    H5TRACE1("i", "*s", name);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, H5I_INVALID_HID, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLregister() */


/*-------------------------------------------------------------------------
 * Function:    H5VLunregister
 *
 * Purpose:     Removes a vol driver ID from the library. This in no way affects
 *              file access property lists which have been defined to use
 *              this vol driver or files which are already opened under with
 *              this driver.
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLunregister(hid_t vol_id)
{
    H5VL_class_t *cls = NULL;
    herr_t ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE1("e", "i", vol_id);

    /* Check arguments */
    if(NULL == (cls = (H5VL_class_t *)H5I_object_verify(vol_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a vol driver")

    /* The H5VL_class_t struct will be freed by this function */
    if(H5I_dec_app_ref(vol_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to unregister vol driver")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLunregister() */


/*-------------------------------------------------------------------------
 * Function:    H5VLinitialize
 *
 * Purpose:     Calls the driver-specific callback to initialize the driver.
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLinitialize(hid_t driver_id, hid_t vipl_id)
{
    H5VL_class_t *cls = NULL;
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "ii", driver_id, vipl_id);

    /* Check args */
    if (NULL == (cls = (H5VL_class_t *)H5I_object_verify(driver_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL driver ID")

    if (cls->initialize && cls->initialize(vipl_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "VOL driver did not initialize")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLinitialize() */


/*-------------------------------------------------------------------------
 * Function:    H5VLterminate
 *
 * Purpose:     Calls the driver-specific callback to terminate the driver.
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLterminate(hid_t driver_id, hid_t vtpl_id)
{
    H5VL_class_t *cls = NULL;
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "ii", driver_id, vtpl_id);

    /* Check args */
    if (NULL == (cls = (H5VL_class_t *)H5I_object_verify(driver_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL driver ID")

    if (cls->terminate && cls->terminate(vtpl_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "VOL driver did not terminate cleanly")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLterminate() */


/*-------------------------------------------------------------------------
 * Function:    H5VLis_registered
 *
 * Purpose:     Tests whether a VOL class has been registered or not
 *
 * Return:      Positive if the VOL class has been registered
 *
 *              Zero if it is unregistered
 *
 *              Negative on error (if the class is not a valid class ID)
 *
 *-------------------------------------------------------------------------
 */
htri_t
H5VLis_registered(const char *name)
{
    H5VL_get_driver_ud_t op_data;
    htri_t ret_value = FALSE;     /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE1("t", "*s", name);

    op_data.found_id = H5I_INVALID_HID;
    op_data.name = name;

    /* Check arguments */
    if(H5I_iterate(H5I_VOL, H5VL__get_driver_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, FAIL, "can't iterate over VOL ids")

    if(op_data.found_id != H5I_INVALID_HID)
        ret_value = TRUE;

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLis_registered() */


/*-------------------------------------------------------------------------
 * Function:    H5VLget_driver_name
 *
 * Purpose:     Returns the driver name for the VOL associated with the
 *              object or file ID
 *
 * Return:      Success:        The length of the driver name
 *
 *              Failure:        Negative
 *
 *-------------------------------------------------------------------------
 */
ssize_t
H5VLget_driver_name(hid_t obj_id, char *name/*out*/, size_t size)
{
    ssize_t    ret_value;

    FUNC_ENTER_API(FAIL)
    H5TRACE3("Zs", "ixz", obj_id, name, size);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, -1, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLget_driver_name() */


/*-------------------------------------------------------------------------
 * Function:    H5VLclose
 *
 * Purpose:     Closes the specified VOL driver.  The VOL ID will no longer
 *              be valid for accessing the VOL.
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLclose(hid_t vol_id)
{
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE1("e", "i", vol_id);

    /* Check args */
    if(NULL == H5I_object_verify(vol_id, H5I_VOL))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL driver ID")

    if(H5I_dec_app_ref(vol_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to close VOL driver ID")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLclose() */


/*---------------------------------------------------------------------------
 * Function:	H5VLobject_register
 *
 * Purpose:     Public routine to create an HDF5 hid_t with library
 *              specific types, bypassing the limitation of H5Iregister.
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
hid_t
H5VLobject_register(void *obj, H5I_type_t obj_type, hid_t driver_id)
{
    hid_t ret_value = FAIL;

    FUNC_ENTER_API(FAIL)
    H5TRACE3("i", "*xIti", obj, obj_type, driver_id);

    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid object to register")

    if ((ret_value = H5VL_object_register(obj, obj_type, driver_id, TRUE)) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to register object")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5VLobject_register */


/*---------------------------------------------------------------------------
 * Function:    H5VLget_object
 *
 * Purpose:     Retrieves the object pointer associated with the ID. This
 *              also optionally returns the H5VL_t struct that this ID
 *              belongs to, if the user passes a valid pointer value.
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLget_object(hid_t obj_id, void **obj)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE2("e", "i**x", obj_id, obj);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5VLget_object */


/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_cancel
 *
 * Purpose:     Cancels a request
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_cancel(void **req, hid_t driver_id, H5ES_status_t *status)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "**xi*Es", req, driver_id, status);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLrequest_cancel() */


/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_test
 *
 * Purpose:     Tests a request
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_test(void **req, hid_t driver_id, H5ES_status_t *status)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "**xi*Es", req, driver_id, status);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLrequest_test() */


/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_wait
 *
 * Purpose:     Waits on a request
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_wait(void **req, hid_t driver_id, H5ES_status_t *status)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "**xi*Es", req, driver_id, status);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLrequest_wait() */


/*-------------------------------------------------------------------------
 * Function:    H5VLattr_create
 *
 * Purpose:     Creates an attribute
 *
 * Return:      Success:    Pointer to the new attribute
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLattr_create(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
                hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_API(NULL)
    H5TRACE8("*x", "*xxi*siii**x", obj, loc_params, driver_id, name, acpl_id,
             aapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_create() */


/*-------------------------------------------------------------------------
 * Function:	H5VLattr_open
 *
 * Purpose:     Opens an attribute
 *
 * Return:      Success:    Pointer to the new attribute
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLattr_open(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
              hid_t aapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_API(NULL)
    H5TRACE7("*x", "*xxi*sii**x", obj, loc_params, driver_id, name, aapl_id,
             dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VLattr_read
 *
 * Purpose:     Reads data from an attribute
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t H5VLattr_read(void *attr, hid_t driver_id, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_read() */


/*-------------------------------------------------------------------------
 * Function:    H5VLattr_write
 *
 * Purpose:     Writes data to an attribute
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t H5VLattr_write(void *attr, hid_t driver_id, hid_t mem_type_id, const void *buf, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_write() */


/*-------------------------------------------------------------------------
 * Function:    H5VLattr_get
 *
 * Purpose:     Gets information about the attribute
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_get(void *obj, hid_t driver_id, H5VL_attr_get_t get_type,
             hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVai**xx", obj, driver_id, get_type, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VLattr_specific
 *
 * Purpose:     Performs a driver-specific operation on an attribute
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_specific(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id,
                  H5VL_attr_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*xxiVbi**xx", obj, loc_params, driver_id, specific_type,
             dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VLattr_optional
 *
 * Purpose:     Performs an optional driver-specific operation on an attribute
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_optional(void *obj, hid_t driver_id, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE5("e", "*xii**xx", obj, driver_id, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VLattr_close
 *
 * Purpose:     Closes an attribute
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_close(void *attr, hid_t driver_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE4("e", "*xii**x", attr, driver_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_create
 *
 * Purpose:     Creates a dataset
 *
 * Return:      Success:    Pointer to the new dataset
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdataset_create(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
                    hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_API(NULL)
    H5TRACE8("*x", "*xxi*siii**x", obj, loc_params, driver_id, name, dcpl_id,
             dapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_open
 *
 * Purpose:     Opens a dataset
 *
 * Return:      Success:    Pointer to the new dataset
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdataset_open(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
                  hid_t dapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE7("*x", "*xxi*sii**x", obj, loc_params, driver_id, name, dapl_id,
             dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_read
 *
 * Purpose:     Reads data from a dataset
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_read(void *dset, hid_t driver_id, hid_t mem_type_id, hid_t mem_space_id,
                  hid_t file_space_id, hid_t plist_id, void *buf, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE8("e", "*xiiiii*x**x", dset, driver_id, mem_type_id, mem_space_id,
             file_space_id, plist_id, buf, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_read() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_write
 *
 * Purpose:     Writes data to a dataset
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_write(void *dset, hid_t driver_id, hid_t mem_type_id, hid_t mem_space_id,
                   hid_t file_space_id, hid_t plist_id, const void *buf, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE8("e", "*xiiiii*x**x", dset, driver_id, mem_type_id, mem_space_id,
             file_space_id, plist_id, buf, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_write() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_get
 *
 * Purpose:     Gets information about a dataset
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_get(void *dset, hid_t driver_id, H5VL_dataset_get_t get_type,
                hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVci**xx", dset, driver_id, get_type, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_specific
 *
 * Purpose:     Performs a driver-specific operation on a dataset
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_specific(void *obj, hid_t driver_id, H5VL_dataset_specific_t specific_type,
                      hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVdi**xx", obj, driver_id, specific_type, dxpl_id, req,
             arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_optional
 *
 * Purpose:     Performs an optional driver-specific operation on a dataset
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_optional(void *obj, hid_t driver_id, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE5("e", "*xii**xx", obj, driver_id, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_close
 *
 * Purpose:     Closes a dataset
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_close(void *dset, hid_t driver_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE4("e", "*xii**x", dset, driver_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VLfile_create
 *
 * Purpose:     Creates a file
 *
 * Return:      Success:    Pointer to the new file
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLfile_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id,
                hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE6("*x", "*sIuiii**x", name, flags, fcpl_id, fapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLfile_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VLfile_open
 *
 * Purpose:     Opens a file
 *
 * Return:      Success:    Pointer to the new file
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLfile_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE5("*x", "*sIuii**x", name, flags, fapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLfile_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VLfile_get
 *
 * Purpose:     Gets information about the file
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_get(void *file, hid_t driver_id, H5VL_file_get_t get_type,
             hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVgi**xx", file, driver_id, get_type, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLfile_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VLfile_specific
 *
 * Purpose:     Performs a driver-specific operation on a file
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_specific(void *file, hid_t driver_id, H5VL_file_specific_t specific_type,
                  hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVhi**xx", file, driver_id, specific_type, dxpl_id, req,
             arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLfile_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VLfile_optional
 *
 * Purpose:     Performs an optional driver-specific operation on a file
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_optional(void *file, hid_t driver_id, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE5("e", "*xii**xx", file, driver_id, dxpl_id, req, arguments);


done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLfile_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VLfile_close
 *
 * Purpose:     Closes a file
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_close(void *file, hid_t driver_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE4("e", "*xii**x", file, driver_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLfile_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_create
 *
 * Purpose:     Creates a group
 *
 * Return:      Success:    Pointer to the new group
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLgroup_create(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
                 hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE8("*x", "*xxi*siii**x", obj, loc_params, driver_id, name, gcpl_id,
             gapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLgroup_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_open
 *
 * Purpose:     Opens a group
 *
 * Return:      Success:    Pointer to the new group
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLgroup_open(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
                hid_t gapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE7("*x", "*xxi*sii**x", obj, loc_params, driver_id, name, gapl_id,
             dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLgroup_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_get
 *
 * Purpose:     Gets information about the group
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_get(void *obj, hid_t driver_id, H5VL_group_get_t get_type,
              hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVii**xx", obj, driver_id, get_type, dxpl_id, req, arguments);


done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLgroup_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_specific
 *
 * Purpose:     Performs a driver-specific operation on a group
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_specific(void *obj, hid_t driver_id, H5VL_group_specific_t specific_type,
                      hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVji**xx", obj, driver_id, specific_type, dxpl_id, req,
             arguments);


done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLgroup_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_optional
 *
 * Purpose:     Performs an optional driver-specific operation on a group
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_optional(void *obj, hid_t driver_id, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE5("e", "*xii**xx", obj, driver_id, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLgroup_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_close
 *
 * Purpose:     Closes a group
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_close(void *grp, hid_t driver_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE4("e", "*xii**x", grp, driver_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLgroup_close() */


/*-------------------------------------------------------------------------
 * Function:    H5VLlink_create
 *
 * Purpose:     Creates a hard link
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_create(H5VL_link_create_type_t create_type, void *obj, H5VL_loc_params_t loc_params,
                 hid_t driver_id, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE8("e", "Vk*xxiiii**x", create_type, obj, loc_params, driver_id, lcpl_id,
             lapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLlink_create() */


/*-------------------------------------------------------------------------
 * Function:    H5VLlink_copy
 *
 * Purpose:     Copies a link to a new location
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_copy(void *src_obj, H5VL_loc_params_t loc_params1, void *dst_obj,
              H5VL_loc_params_t loc_params2, hid_t driver_id,
              hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE9("e", "*xx*xxiiii**x", src_obj, loc_params1, dst_obj, loc_params2,
             driver_id, lcpl_id, lapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLlink_copy() */


/*-------------------------------------------------------------------------
 * Function:    H5VLlink_move
 *
 * Purpose:     Moves a link to another location
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_move(void *src_obj, H5VL_loc_params_t loc_params1, void *dst_obj,
              H5VL_loc_params_t loc_params2, hid_t driver_id,
              hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE9("e", "*xx*xxiiii**x", src_obj, loc_params1, dst_obj, loc_params2,
             driver_id, lcpl_id, lapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLlink_move() */


/*-------------------------------------------------------------------------
 * Function:    H5VLlink_get
 *
 * Purpose:     Gets information about a link
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_get(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, H5VL_link_get_t get_type,
              hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*xxiVli**xx", obj, loc_params, driver_id, get_type, dxpl_id, req,
             arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLlink_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VLlink_specific
 *
 * Purpose:     Performs a driver-specific operation on a link
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_specific(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id,
                  H5VL_link_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*xxiVmi**xx", obj, loc_params, driver_id, specific_type,
             dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLlink_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VLlink_optional
 *
 * Purpose:     Performs an optional driver-specific operation on a link
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_optional(void *obj, hid_t driver_id, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE5("e", "*xii**xx", obj, driver_id, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLlink_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VLobject_open
 *
 * Purpose:     Opens an object
 *
 * Return:      Success:    Pointer to the new object
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLobject_open(void *obj, H5VL_loc_params_t params, hid_t driver_id, H5I_type_t *opened_type,
                hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE6("*x", "*xxi*Iti**x", obj, params, driver_id, opened_type, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLobject_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VLobject_copy
 *
 * Purpose:     Copies an object to another location
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_copy(void *src_obj, H5VL_loc_params_t loc_params1, hid_t driver_id1, const char *src_name,
                void *dst_obj, H5VL_loc_params_t loc_params2, hid_t driver_id2, const char *dst_name,
                hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE12("e", "*xxi*s*xxi*siii**x", src_obj, loc_params1, driver_id1,
             src_name, dst_obj, loc_params2, driver_id2, dst_name, ocpypl_id,
             lcpl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLobject_copy() */


/*-------------------------------------------------------------------------
 * Function:    H5VLobject_get
 *
 * Purpose:     Gets information about an object
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_get(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, H5VL_object_get_t get_type,
               hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*xxiVni**xx", obj, loc_params, driver_id, get_type, dxpl_id, req,
             arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLobject_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VLobject_specific
 *
 * Purpose:     Performs a driver-specific operation on an object
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_specific(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id,
                    H5VL_object_specific_t specific_type, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*xxiVoi**xx", obj, loc_params, driver_id, specific_type,
             dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLobject_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VLobject_optional
 *
 * Purpose:     Performs an optional driver-specific operation on an object
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_optional(void *obj, hid_t driver_id, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE5("e", "*xii**xx", obj, driver_id, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLobject_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_commit
 *
 * Purpose:     Commits a datatype to the file
 *
 * Return:      Success:    Pointer to the new datatype
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdatatype_commit(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
                     hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE10("*x", "*xxi*siiiii**x", obj, loc_params, driver_id, name, type_id,
             lcpl_id, tcpl_id, tapl_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdatatype_commit() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_open
 *
 * Purpose:     Opens a named datatype
 *
 * Return:      Success:    Pointer to the new datatype
 *
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdatatype_open(void *obj, H5VL_loc_params_t loc_params, hid_t driver_id, const char *name,
                   hid_t tapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL;

    FUNC_ENTER_API(NULL)
    H5TRACE7("*x", "*xxi*sii**x", obj, loc_params, driver_id, name, tapl_id,
             dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdatatype_open() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_specific
 *
 * Purpose:     Performs a driver-specific operation on a datatype
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_specific(void *obj, hid_t driver_id, H5VL_datatype_specific_t specific_type,
                      hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVfi**xx", obj, driver_id, specific_type, dxpl_id, req,
             arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdatatype_specific() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_optional
 *
 * Purpose:     Performs an optional driver-specific operation on a datatype
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_optional(void *obj, hid_t driver_id, hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE5("e", "*xii**xx", obj, driver_id, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdatatype_optional() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_get
 *
 * Purpose:     Gets information about the datatype
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_get(void *obj, hid_t driver_id, H5VL_datatype_get_t get_type,
                 hid_t dxpl_id, void **req, va_list arguments)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE6("e", "*xiVei**xx", obj, driver_id, get_type, dxpl_id, req, arguments);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdatatype_get() */


/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_close
 *
 * Purpose:     Closes a datatype
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_close(void *dt, hid_t driver_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)
    H5TRACE4("e", "*xii**x", dt, driver_id, dxpl_id, req);

    HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unimplemented VOL function")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdatatype_close() */

